#include "NLHideDoor.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "NLRunner.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogNLDoor, Log, All);

namespace
{
	// Motorun 100 cm'lik ortalanmış küpü: verilen ölçüye (cm) göre ölçek
	FVector CubeScale(float X, float Y, float Z) { return FVector(X, Y, Z) / 100.f; }
}

ANLHideDoor::ANLHideDoor()
{
	NL_DEFAULT_SOUND(OpenSound, "door_open");
	NL_DEFAULT_SOUND(CloseSound, "door_close");
	NL_DEFAULT_SOUND(BangSound, "door_bang");
	NL_DEFAULT_SOUND(HandleSound, "door_handle");
	NL_DEFAULT_SOUND(BurstSound, "door_burst");
	NL_DEFAULT_SOUND(HoldSuccessSound, "runner_retreat");
	NL_DEFAULT_SOUND(PanicBreathLoop, "breath_panic_loop");

	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto MakeCube = [this](const TCHAR* Name, USceneComponent* Parent) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(CubeFinder.Get());
		return C;
	};

	FillLeft = MakeCube(TEXT("FillLeft"), Root);
	FillRight = MakeCube(TEXT("FillRight"), Root);
	FillTop = MakeCube(TEXT("FillTop"), Root);
	FrameParts.Add(MakeCube(TEXT("FrameLeft"), Root));
	FrameParts.Add(MakeCube(TEXT("FrameRight"), Root));
	FrameParts.Add(MakeCube(TEXT("FrameTop"), Root));
	for (UStaticMeshComponent* F : FrameParts) F->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	DoorMesh = MakeCube(TEXT("DoorMesh"), Hinge);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAll"));
	DoorMesh->SetCanEverAffectNavigation(false);   // kapı açık/kapalı fark etmeksizin NavMesh geçitten geçsin

	// İki kol (iç ve dış): gül + sap + yatay kol. Pivot döner = kol aşağı basılır.
	HandlePivotIn = CreateDefaultSubobject<USceneComponent>(TEXT("HandlePivotIn"));
	HandlePivotIn->SetupAttachment(Hinge);
	HandlePivotOut = CreateDefaultSubobject<USceneComponent>(TEXT("HandlePivotOut"));
	HandlePivotOut->SetupAttachment(Hinge);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		USceneComponent* Pivot = Side == 0 ? HandlePivotIn.Get() : HandlePivotOut.Get();
		const TCHAR* Tag = Side == 0 ? TEXT("In") : TEXT("Out");

		UStaticMeshComponent* Rose = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HandleRose%s"), Tag));
		Rose->SetupAttachment(Pivot);
		Rose->SetStaticMesh(CylFinder.Get());
		HandleParts.Add(Rose);

		UStaticMeshComponent* Stem = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HandleStem%s"), Tag));
		Stem->SetupAttachment(Pivot);
		Stem->SetStaticMesh(CylFinder.Get());
		HandleParts.Add(Stem);

		UStaticMeshComponent* Lever = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HandleLever%s"), Tag));
		Lever->SetupAttachment(Pivot);
		Lever->SetStaticMesh(CubeFinder.Get());
		HandleParts.Add(Lever);
	}
	for (UStaticMeshComponent* P : HandleParts)
	{
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P->SetCastShadow(false);
	}

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(Root);
	BreathAudio->bAutoActivate = false;
	BreathAudio->bAllowSpatialization = false;   // oyuncunun kendi nefesi

	LayoutParts();
}

void ANLHideDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	bWantOpen = bStartOpen;
	OpenAlpha = bStartOpen ? 1.f : 0.f;
	LayoutParts();
}

void ANLHideDoor::LayoutParts()
{
	const float HalfW = OpeningWidth * 0.5f;
	const float HalfDoor = DoorWidth * 0.5f;
	const float SideW = FMath::Max(HalfW - HalfDoor, 1.f);

	// Dolgu duvarlar: kapının iki yanı ve üstü
	FillLeft->SetRelativeLocation(FVector(0.f, -(HalfDoor + SideW * 0.5f), OpeningHeight * 0.5f));
	FillLeft->SetRelativeScale3D(CubeScale(WallThickness, SideW, OpeningHeight));
	FillRight->SetRelativeLocation(FVector(0.f, HalfDoor + SideW * 0.5f, OpeningHeight * 0.5f));
	FillRight->SetRelativeScale3D(CubeScale(WallThickness, SideW, OpeningHeight));
	const float TopH = FMath::Max(OpeningHeight - DoorHeight, 1.f);
	FillTop->SetRelativeLocation(FVector(0.f, 0.f, DoorHeight + TopH * 0.5f));
	FillTop->SetRelativeScale3D(CubeScale(WallThickness, DoorWidth, TopH));

	// Kasa (pervaz): duvardan biraz taşar
	const float Trim = 9.f, TrimDepth = WallThickness + 4.f;
	FrameParts[0]->SetRelativeLocation(FVector(0.f, -(HalfDoor + Trim * 0.5f - 2.f), DoorHeight * 0.5f));
	FrameParts[0]->SetRelativeScale3D(CubeScale(TrimDepth, Trim, DoorHeight));
	FrameParts[1]->SetRelativeLocation(FVector(0.f, HalfDoor + Trim * 0.5f - 2.f, DoorHeight * 0.5f));
	FrameParts[1]->SetRelativeScale3D(CubeScale(TrimDepth, Trim, DoorHeight));
	FrameParts[2]->SetRelativeLocation(FVector(0.f, 0.f, DoorHeight + Trim * 0.5f - 2.f));
	FrameParts[2]->SetRelativeScale3D(CubeScale(TrimDepth, DoorWidth + Trim * 2.f, Trim));

	// Menteşe kapının sol kenarında; kapı oradan +Y yönüne uzanır. Kapı duvarın oda tarafındaki yüzüyle aynı hizada.
	Hinge->SetRelativeLocation(FVector(GetDoorPlaneX(), -HalfDoor, 0.f));
	DoorMesh->SetRelativeLocation(FVector(0.f, HalfDoor, DoorHeight * 0.5f + 0.5f));
	DoorMesh->SetRelativeScale3D(CubeScale(DoorThickness, DoorWidth - 1.f, DoorHeight - 1.f));

	const float HandleY = DoorWidth - 12.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Dir = Side == 0 ? 1.f : -1.f;   // iç = +X
		USceneComponent* Pivot = Side == 0 ? HandlePivotIn.Get() : HandlePivotOut.Get();
		Pivot->SetRelativeLocation(FVector(Dir * (DoorThickness * 0.5f), HandleY, HandleHeight));

		UStaticMeshComponent* Rose = HandleParts[Side * 3 + 0];
		UStaticMeshComponent* Stem = HandleParts[Side * 3 + 1];
		UStaticMeshComponent* Lever = HandleParts[Side * 3 + 2];
		// Silindir ekseni Z: X eksenine yatır
		Rose->SetRelativeLocationAndRotation(FVector(Dir * 0.8f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
		Rose->SetRelativeScale3D(FVector(0.065f, 0.065f, 0.016f));
		Stem->SetRelativeLocationAndRotation(FVector(Dir * 3.5f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f));
		Stem->SetRelativeScale3D(FVector(0.022f, 0.022f, 0.05f));
		// Yatay kol menteşeye doğru (-Y) uzanır
		Lever->SetRelativeLocation(FVector(Dir * 6.f, -6.f, 0.f));
		Lever->SetRelativeScale3D(CubeScale(2.4f, 13.f, 2.2f));
	}

	ApplyDoorRotation();
}

void ANLHideDoor::BeginPlay()
{
	Super::BeginPlay();
	bWantOpen = bStartOpen;
	OpenAlpha = bStartOpen ? 1.f : 0.f;
	LayoutParts();
	if (PanicBreathLoop) BreathAudio->SetSound(PanicBreathLoop);

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto Paint = [this, Base](UStaticMeshComponent* C, const FLinearColor& Color)
	{
		if (!C || !Base) return;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
	};
	Paint(DoorMesh, FLinearColor(0.16f, 0.085f, 0.045f));          // koyu ahşap
	for (UStaticMeshComponent* F : FrameParts) Paint(F, FLinearColor(0.1f, 0.06f, 0.035f));
	for (UStaticMeshComponent* H : HandleParts) Paint(H, FLinearColor(0.55f, 0.52f, 0.45f));
	const FLinearColor Wall(0.32f, 0.31f, 0.29f);
	Paint(FillLeft, Wall);
	Paint(FillRight, Wall);
	Paint(FillTop, Wall);
}

void ANLHideDoor::ApplyDoorRotation()
{
	// İçeri (+X) doğru açılır: menteşe etrafında negatif yaw
	const float Ease = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp(OpenAlpha, 0.f, 1.f), 2.f);
	Hinge->SetRelativeRotation(FRotator(0.f, -(OpenAngle * Ease + Rattle), 0.f));

	// Kol aşağı basılır (kapının normali etrafında döner)
	const FRotator HandleRot(0.f, 0.f, -42.f * HandleDown);
	HandlePivotIn->SetRelativeRotation(HandleRot);
	HandlePivotOut->SetRelativeRotation(HandleRot);
}

// ------------------------------------------------------------------ Etkileşim

bool ANLHideDoor::CanInteract(const ANLPlayerCharacter* Player) const
{
	return Phase == EHoldPhase::None;
}

FText ANLHideDoor::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return FText::FromString(bWantOpen ? TEXT("[E] Close door") : TEXT("[E] Open door"));
}

void ANLHideDoor::Interact(ANLPlayerCharacter* Player)
{
	if (Phase != EHoldPhase::None) return;
	bWantOpen = !bWantOpen;
	if (bWantOpen)
	{
		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetHandleGripLocation());
		bWasClosing = false;
	}
	else
	{
		bWasClosing = true;
		ClosingPlayer = Player;
	}
}

bool ANLHideDoor::IsPlayerInside(const ANLPlayerCharacter* Player) const
{
	if (!Player) return false;
	const FVector Local = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
	return Local.X > GetDoorPlaneX() + 10.f && FMath::Abs(Local.Y) < OpeningWidth * 2.f + 400.f;
}

ANLRunner* ANLHideDoor::FindChasingRunner() const
{
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
	{
		if (It->IsChasing()) return *It;
	}
	return nullptr;
}

// ------------------------------------------------------------------ Konumlar

FVector ANLHideDoor::GetHandleGripLocation() const
{
	return HandlePivotIn->GetComponentTransform().TransformPosition(FVector(6.f, -7.f, 0.f));
}

FVector ANLHideDoor::GetDoorPalmLocation() const
{
	return Hinge->GetComponentTransform().TransformPosition(FVector(DoorThickness * 0.5f + 3.f, DoorWidth * 0.42f, HandleHeight + 38.f));
}

FVector ANLHideDoor::GetHoldStandLocation(float CapsuleHalfHeight) const
{
	const FVector Floor = GetActorTransform().TransformPosition(FVector(GetDoorPlaneX() + 40.f, DoorWidth * 0.5f - 30.f, 0.f));
	return Floor + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
}

FVector ANLHideDoor::GetOutsideLocation() const
{
	return GetActorTransform().TransformPosition(FVector(-(WallThickness * 0.5f + 45.f), 15.f, 0.f));
}

// ------------------------------------------------------------------ Tick

void ANLHideDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// --- Kapı açılıp kapanması ---
	const float Speed = Phase == EHoldPhase::Burst ? 7.f : OpenSpeed;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, bWantOpen ? 1.f : 0.f, DeltaSeconds, Speed);
	if (bWasClosing && OpenAlpha <= 0.f)
	{
		bWasClosing = false;
		UGameplayStatics::PlaySoundAtLocation(this, CloseSound, GetHandleGripLocation());

		// Runner kovalarken içeride kapattıysa: kapıyı tutma
		ANLPlayerCharacter* Player = ClosingPlayer.Get();
		ANLRunner* Runner = FindChasingRunner();
		UE_LOG(LogNLDoor, Log, TEXT("Door closed: player inside=%d, runner chasing=%d"), IsPlayerInside(Player) ? 1 : 0, Runner ? 1 : 0);
		if (Player && Runner && IsPlayerInside(Player) && Player->GetMode() == ENLPlayerMode::Walking)
		{
			BeginHold(Player, Runner);
		}
	}

	// --- Kapı tutma ---
	float WantHandle = 0.f, WantRattle = 0.f, WantForce = 0.f;
	if (Phase != EHoldPhase::None)
	{
		PhaseTimer += DeltaSeconds;
		const float T = GetWorld()->GetTimeSeconds();

		switch (Phase)
		{
		case EHoldPhase::Settle:
		case EHoldPhase::Gap:
			if (PhaseTimer >= PhaseLength) StartCheck();
			break;

		case EHoldPhase::Force:
		case EHoldPhase::Check:
			// Dışarıdan zorluyor: kol aşağı basılı titrer, kapı aralanıp çarpar
			WantForce = 1.f;
			WantHandle = 0.8f + 0.2f * FMath::Sin(T * 31.f);
			WantRattle = 2.2f + 1.6f * FMath::Abs(FMath::Sin(T * 9.f)) + FMath::FRandRange(-0.4f, 0.4f);
			if (Phase == EHoldPhase::Force)
			{
				if (PhaseTimer >= PhaseLength) SetPhase(EHoldPhase::Check, PhaseLength);
			}
			else
			{
				const float Sweep = FMath::Max(SweepTime * (1.f - SweepSpeedUp * ChecksPassed), 0.6f);
				const float NewPos = FMath::Min(NeedlePos + DeltaSeconds / Sweep, 1.f);
				if (DebugAutoResult > 0 && NewPos >= ZoneStart + ZoneWidth * 0.5f)
				{
					// Test: bölgenin ortasında basılmış say
					NeedlePos = ZoneStart + ZoneWidth * 0.5f;
					PassCheck();
					break;
				}
				NeedlePos = NewPos;
				if (NeedlePos >= 1.f)
				{
					FailHold();   // zamanında basılmadı
				}
			}
			break;

		case EHoldPhase::Release:
			if (PhaseTimer >= PhaseLength) FinishHold();
			break;

		case EHoldPhase::Burst:
			// Kapı içeri savrulur: oyuncu geriye itilir (kapı kameranın önünden geçmesin, yaratık karşıda belirsin)
			if (ANLPlayerCharacter* P = HoldPlayer.Get())
			{
				const float K = FMath::Clamp(PhaseTimer / 0.12f, 0.f, 1.f);
				P->SetActorLocation(BurstFrom + GetActorForwardVector() * (BurstKnockback * (1.f - FMath::Square(1.f - K))));
				// Savrulma bitince (kamera yeni yerine geçsin diye bir kare sonra) yakalar: yaratık kameranın tam önüne konur
				if (K >= 1.f) ++BurstSettledFrames;
				if (BurstSettledFrames >= 2 && !bBurstKillDone)
				{
					bBurstKillDone = true;
					if (ANLRunner* Runner = HoldRunner.Get()) Runner->DoorKill(P);
					else P->Die();
				}
			}
			if (PhaseTimer >= PhaseLength) EndHoldCommon();
			break;

		default:
			break;
		}
	}

	ForceAlpha = FMath::FInterpTo(ForceAlpha, WantForce, DeltaSeconds, WantForce > ForceAlpha ? 18.f : 3.f);
	HandleDown = FMath::FInterpTo(HandleDown, WantHandle, DeltaSeconds, WantHandle > HandleDown ? 25.f : 12.f);
	Rattle = Phase == EHoldPhase::Burst ? 0.f : FMath::FInterpTo(Rattle, WantRattle, DeltaSeconds, 20.f);
	ApplyDoorRotation();
}

// ------------------------------------------------------------------ Kapı tutma akışı

void ANLHideDoor::BeginHold(ANLPlayerCharacter* Player, ANLRunner* Runner)
{
	HoldPlayer = Player;
	HoldRunner = Runner;
	ChecksPassed = 0;
	NeedlePos = 0.f;
	UE_LOG(LogNLDoor, Log, TEXT("Hold the door: started (%d checks)"), NumChecks);

	Player->EnterDoorHold(this);
	if (Runner) Runner->BeginDoorAssault(this);
	BreathAudio->Play();
	BreathAudio->SetVolumeMultiplier(1.f);
	SetPhase(EHoldPhase::Settle, SettleTime);
}

void ANLHideDoor::DebugStartHold(ANLPlayerCharacter* Player, int32 AutoResult)
{
	DebugAutoResult = AutoResult;
	bWantOpen = false;
	OpenAlpha = 0.f;
	bWasClosing = false;
	ANLRunner* Runner = nullptr;
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) { Runner = *It; break; }
	BeginHold(Player, Runner);
}

void ANLHideDoor::SetPhase(EHoldPhase NewPhase, float Length)
{
	Phase = NewPhase;
	PhaseTimer = 0.f;
	PhaseLength = Length;
}

void ANLHideDoor::StartCheck()
{
	// Runner kapıya yüklenir: darbe + kol takırtısı. Kısa bir an sonra ibre çıkar.
	ZoneWidth = FMath::Max(ZoneWidthMin, ZoneWidthStart - 0.025f * ChecksPassed);
	ZoneStart = FMath::FRandRange(0.35f, 0.92f - ZoneWidth);
	NeedlePos = 0.f;
	UGameplayStatics::PlaySoundAtLocation(this, BangSound, GetHandleGripLocation());
	UGameplayStatics::PlaySoundAtLocation(this, HandleSound, GetHandleGripLocation());
	UE_LOG(LogNLDoor, Log, TEXT("Hold the door: check %d/%d (zone %.2f-%.2f)"), ChecksPassed + 1, NumChecks, ZoneStart, ZoneStart + ZoneWidth);
	SetPhase(EHoldPhase::Force, ForceLeadTime);
}

void ANLHideDoor::HandleHoldInput()
{
	if (Phase != EHoldPhase::Check) return;
	const bool bInZone = NeedlePos >= ZoneStart && NeedlePos <= ZoneStart + ZoneWidth;
	if (bInZone) PassCheck();
	else FailHold();
}

void ANLHideDoor::PassCheck()
{
	++ChecksPassed;
	UE_LOG(LogNLDoor, Log, TEXT("Hold the door: check passed (%d/%d)"), ChecksPassed, NumChecks);
	// Kapı geri itildi: kapanma sesi, kol yukarı fırlar
	UGameplayStatics::PlaySoundAtLocation(this, CloseSound, GetHandleGripLocation(), 0.6f, 1.15f);
	if (ChecksPassed >= NumChecks)
	{
		// Runner bırakıp gider
		if (ANLRunner* Runner = HoldRunner.Get()) Runner->LeaveDoor();
		UGameplayStatics::PlaySoundAtLocation(this, HoldSuccessSound, GetOutsideLocation());
		SetPhase(EHoldPhase::Release, 3.f);
	}
	else
	{
		SetPhase(EHoldPhase::Gap, FMath::FRandRange(GapMin, GapMax));
	}
}

void ANLHideDoor::FailHold()
{
	UE_LOG(LogNLDoor, Log, TEXT("Hold the door: FAILED, the Runner breaks in"));
	// Kapı içeri doğru zorla açılır, Runner jumpscare
	bWantOpen = true;
	UGameplayStatics::PlaySoundAtLocation(this, BurstSound, GetHandleGripLocation());
	BreathAudio->Stop();
	ANLPlayerCharacter* Player = HoldPlayer.Get();
	if (Player) BurstFrom = Player->GetActorLocation();
	bBurstKillDone = false;
	BurstSettledFrames = 0;
	SetPhase(EHoldPhase::Burst, 0.6f);
}

void ANLHideDoor::FinishHold()
{
	UE_LOG(LogNLDoor, Log, TEXT("Hold the door: survived, the Runner left"));
	if (ANLPlayerCharacter* Player = HoldPlayer.Get()) Player->ExitDoorHold();
	EndHoldCommon();
}

void ANLHideDoor::EndHoldCommon()
{
	BreathAudio->FadeOut(2.5f, 0.f);
	SetPhase(EHoldPhase::None, 0.f);
	HoldPlayer.Reset();
	HoldRunner.Reset();
	DebugAutoResult = 0;
}
