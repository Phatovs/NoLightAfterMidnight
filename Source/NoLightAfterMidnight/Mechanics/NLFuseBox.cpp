#include "NLFuseBox.h"
#include "NLDefaults.h"
#include "NLPowerSubsystem.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "NLSaboteur.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"

namespace NLFuseLayout
{
	// Kutu (SM_Cube, köşe pivotlu 100 cm) yerel koordinatları; ön yüz +Y (y = 100). Panel kutunun soluna (-X) uzanır.
	static const FVector SlotPos[ANLFuseBox::NumFuses] =
	{
		FVector(-65.f, 105.f, 72.f), FVector(-25.f, 105.f, 72.f),   // üst sıra: sol, sağ
		FVector(-65.f, 105.f, 28.f), FVector(-25.f, 105.f, 28.f)    // alt sıra: sol, sağ
	};
}

ANLFuseBox::ANLFuseBox()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(SwitchOnSound, "fuse_switch_on");
	NL_DEFAULT_SOUND(SwitchOffSound, "fuse_switch_off");
	NL_DEFAULT_SOUND(TripSound, "fuse_trip");
	NL_DEFAULT_SOUND(FuseMoveSound, "charger_dock");

	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = BoxMesh;

	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	LeverMesh->SetupAttachment(BoxMesh);

	// Yakın plan kamera: panel + şalter kadrajda (kutu yerel koordinatlarında, ön yüze bakar)
	FuseViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FuseViewCamera"));
	FuseViewCamera->SetupAttachment(BoxMesh);
	// Panel (x -90..0) ve şalter (x ~50) ortalanır
	FuseViewCamera->SetRelativeLocationAndRotation(FVector(-15.f, 265.f, 58.f), FRotator(-5.f, -90.f, 0.f));
	FuseViewCamera->SetFieldOfView(62.f);

	ViewLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FuseViewLight"));
	ViewLight->SetupAttachment(BoxMesh);
	ViewLight->SetRelativeLocation(FVector(-15.f, 150.f, 125.f));
	ViewLight->SetIntensity(0.f);
	ViewLight->SetAttenuationRadius(260.f);
	ViewLight->SetLightColor(FLinearColor(0.85f, 0.9f, 1.f));
	ViewLight->SetCastShadows(false);
	FuseViewCamera->bAutoActivate = true;

	// Sigorta paneli: kutunun soluna doğru büyür
	PanelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FusePanel"));
	PanelMesh->SetupAttachment(BoxMesh);
	PanelMesh->SetStaticMesh(CubeFinder.Get());
	PanelMesh->SetRelativeLocation(FVector(-45.f, 50.f, 50.f));
	PanelMesh->SetRelativeScale3D(FVector(0.9f, 1.f, 1.f));

	for (int32 i = 0; i < NumFuses; ++i)
	{
		const FVector P = NLFuseLayout::SlotPos[i];

		// Yuva plakası: doğru sigortanın rengi (oyuncu hangi sigortanın nereye gittiğini buradan anlar)
		UStaticMeshComponent* Plate = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FuseSlotPlate%d"), i));
		Plate->SetupAttachment(BoxMesh);
		Plate->SetStaticMesh(CubeFinder.Get());
		Plate->SetRelativeLocation(FVector(P.X, 100.6f, P.Z));
		Plate->SetRelativeScale3D(FVector(0.16f, 0.012f, 0.34f));
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SlotPlates.Add(Plate);

		// Dikey cam sigorta (renkli gövde) + iki metal başlık
		UStaticMeshComponent* Body = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FuseBody%d"), i));
		Body->SetupAttachment(BoxMesh);
		Body->SetStaticMesh(CylFinder.Get());
		Body->SetRelativeLocation(P);
		Body->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.2f));
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FuseBodies.Add(Body);

		for (int32 c = 0; c < 2; ++c)
		{
			UStaticMeshComponent* Cap = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FuseCap%d_%d"), i, c));
			Cap->SetupAttachment(BoxMesh);
			Cap->SetStaticMesh(CylFinder.Get());
			Cap->SetRelativeLocation(P + FVector(0.f, 0.f, c == 0 ? 10.f : -10.f));
			Cap->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.045f));
			Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			FuseCaps.Add(Cap);
		}
	}
}

FLinearColor ANLFuseBox::GetFuseColor(int32 Fuse)
{
	static const FLinearColor Colors[NumFuses] =
	{
		FLinearColor(0.85f, 0.08f, 0.06f),  // kırmızı
		FLinearColor(0.06f, 0.28f, 0.95f),  // mavi
		FLinearColor(0.08f, 0.75f, 0.18f),  // yeşil
		FLinearColor(0.95f, 0.78f, 0.05f)   // sarı
	};
	return Colors[FMath::Clamp(Fuse, 0, NumFuses - 1)];
}

FText ANLFuseBox::GetFuseColorName(int32 Fuse)
{
	static const TCHAR* Names[NumFuses] = { TEXT("RED"), TEXT("BLUE"), TEXT("GREEN"), TEXT("YELLOW") };
	return FText::FromString(Names[FMath::Clamp(Fuse, 0, NumFuses - 1)]);
}

void ANLFuseBox::BeginPlay()
{
	Super::BeginPlay();
	bOn = bStartOn;

	SlotFuse.SetNum(NumFuses);
	for (int32 i = 0; i < NumFuses; ++i) SlotFuse[i] = i;
	SlotTarget = SlotFuse;

	// Renkler: BasicShapeMaterial "Color" parametresi
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	auto MakeMID = [this, Base](UStaticMeshComponent* C, const FLinearColor& Color) -> UMaterialInstanceDynamic*
	{
		if (!C || !Base) return nullptr;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		C->SetMaterial(0, MID);
		return MID;
	};
	MakeMID(PanelMesh, FLinearColor(0.09f, 0.09f, 0.1f));
	for (int32 i = 0; i < SlotPlates.Num(); ++i)
	{
		// Yuvalar renksiz: hangi rengin nereye gittiği sadece masadaki monitörde yazar
		MakeMID(SlotPlates[i], FLinearColor(0.16f, 0.16f, 0.17f));
	}
	for (UStaticMeshComponent* Cap : FuseCaps)
	{
		MakeMID(Cap, FLinearColor(0.55f, 0.55f, 0.58f));
	}
	FuseMIDs.Reset();
	for (UStaticMeshComponent* Body : FuseBodies)
	{
		FuseMIDs.Add(MakeMID(Body, FLinearColor::White));
	}

	ApplyVisuals();
	ApplyFuseVisuals();
	ScheduleNextTrip();
	bWasPowerOk = IsPowerOk();

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RegisterFuseBox(this);
	}
}

bool ANLFuseBox::AreFusesCorrect() const
{
	for (int32 i = 0; i < SlotFuse.Num(); ++i)
	{
		if (SlotFuse[i] != GetTargetFuse(i)) return false;
	}
	return true;
}

void ANLFuseBox::Interact(ANLPlayerCharacter* Player)
{
	// Sigorta kutusu arayüzünü aç (şalter ve sigortalar orada)
	if (Player) Player->OpenFuseBox(this);
}

FText ANLFuseBox::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return FText::FromString(TEXT("[E] Open fuse box"));
}

void ANLFuseBox::SetOn(bool bNewOn)
{
	if (bOn == bNewOn) return;

	bOn = bNewOn;
	UGameplayStatics::PlaySoundAtLocation(this, bOn ? SwitchOnSound : SwitchOffSound, GetActorLocation());
	ApplyVisuals();
	OnSwitched.Broadcast(bOn);
	RefreshPower();

	// Sigortalar yanlış diziliyken şalter kaldırıldı: kıvılcım, kol geri düşer
	if (bOn && !AreFusesCorrect() && HasActorBegunPlay())
	{
		UE_LOG(LogTemp, Log, TEXT("NLFuseBox: switch raised with wrong wiring -> kicks back"));
		TWeakObjectPtr<ANLFuseBox> WeakThis(this);
		GetWorldTimerManager().SetTimer(KickbackTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			ANLFuseBox* B = WeakThis.Get();
			if (!B || !B->bOn || B->AreFusesCorrect()) return;
			if (UNLPowerSubsystem* P = B->GetWorld()->GetSubsystem<UNLPowerSubsystem>()) P->PlayFarEvent(B->TripSound, B->GetActorLocation());
			B->SetOn(false);
		}), FMath::Max(0.05f, WrongWiringKickback), false);
	}
}

void ANLFuseBox::SwapFuses(int32 SlotA, int32 SlotB)
{
	if (!SlotFuse.IsValidIndex(SlotA) || !SlotFuse.IsValidIndex(SlotB) || SlotA == SlotB) return;
	SlotFuse.Swap(SlotA, SlotB);
	UGameplayStatics::PlaySoundAtLocation(this, FuseMoveSound, GetActorLocation());
	ApplyFuseVisuals();
	RefreshPower();
}

void ANLFuseBox::Sabotage()
{
	SabotageWith(FMath::FRand() < SabotageSwitchOffChance);
}

void ANLFuseBox::SabotageWith(bool bSwitchOff)
{
	// Sigortaları karıştır (doğru dizilime denk gelmesin)
	if (SlotFuse.Num() != NumFuses) return;
	// Kablolamayı değiştir: yeni doğru sıra (monitörde görünür)
	if (bRandomizeWiring && SlotTarget.Num() == NumFuses)
	{
		// Bir öncekiyle aynı sıra çıkmasın
		const TArray<int32> Prev = SlotTarget;
		do
		{
			for (int32 i = NumFuses - 1; i > 0; --i) SlotTarget.Swap(i, FMath::RandRange(0, i));
		} while (SlotTarget == Prev);
	}
	do
	{
		for (int32 i = NumFuses - 1; i > 0; --i)
		{
			SlotFuse.Swap(i, FMath::RandRange(0, i));
		}
	} while (AreFusesCorrect());

	// Şalteri bazen indirir, bazen açık bırakır (açıkken dokunan elektrik çarpar)
	if (bSwitchOff && bOn)
	{
		bOn = false;
		ApplyVisuals();
		OnSwitched.Broadcast(bOn);
	}

	UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
	if (Power) Power->MarkFailureActive();
	UE_LOG(LogTemp, Log, TEXT("NLFuseBox: fuses swapped by saboteur, switch left %s, new wiring [%d %d %d %d] (game time %.0f s)"), bOn ? TEXT("ON") : TEXT("OFF"), SlotTarget[0], SlotTarget[1], SlotTarget[2], SlotTarget[3], GetWorld()->GetTimeSeconds());
	// Sabotaj: kıvılcım + şalter düşmesi kulübe yönünden adanın her yerinden duyulur
	if (Power) Power->PlayFarEvent(TripSound, GetActorLocation());
	if (bSwitchOff && Power) Power->PlayFarEvent(SwitchOffSound, GetActorLocation(), 0.8f);
	ApplyFuseVisuals();
	RefreshPower();
}

void ANLFuseBox::RefreshPower()
{
	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RefreshPower();
	}
}

void ANLFuseBox::ApplyVisuals()
{
	// Oyun başında anında yerine koy; sonrasında Tick yumuşakça döndürür
	if (!HasActorBegunPlay())
	{
		LeverAngle = bOn ? LeverOnAngle : LeverOffAngle;
	}
	PlaceLever(LeverAngle);
}

void ANLFuseBox::ApplyFuseVisuals()
{
	for (int32 i = 0; i < FuseMIDs.Num(); ++i)
	{
		if (FuseMIDs[i]) FuseMIDs[i]->SetVectorParameterValue(TEXT("Color"), GetFuseColor(GetFuseInSlot(i)));
	}
}

void ANLFuseBox::PlaceLever(float AngleDeg)
{
	// Kol mafsaldan dışarı çıkar ve yukarı (açık) / aşağı (kapalı) bakar
	const FVector Out = LeverOutward.GetSafeNormal().IsNearlyZero() ? FVector(0.f, 1.f, 0.f) : LeverOutward.GetSafeNormal();
	const float Rad = FMath::DegreesToRadians(AngleDeg);
	const FVector Dir = (Out * FMath::Cos(Rad) + FVector::UpVector * FMath::Sin(Rad)).GetSafeNormal();
	const FQuat Rot = FQuat::FindBetweenNormals(FVector::UpVector, Dir);  // kol mesh'inin uzun ekseni Z
	LeverMesh->SetRelativeLocationAndRotation(LeverPivot + Dir * (LeverLength * 0.5f), Rot);
}

void ANLFuseBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Kol hedef konuma döner
	const float TargetAngle = bOn ? LeverOnAngle : LeverOffAngle;
	if (!bLeverDragging && !FMath::IsNearlyEqual(LeverAngle, TargetAngle))
	{
		LeverAngle = FMath::FInterpConstantTo(LeverAngle, TargetAngle, DeltaSeconds, LeverSpeed);
		PlaceLever(LeverAngle);
	}

	// Sigortalar karışıkken diğer arızalar (jeneratör) bekler
	if (!AreFusesCorrect())
	{
		if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	}

	// Elektrik geri geldi (sigortalar doğru + şalter açık): bir sonraki sabotaj sayacı baştan
	const bool bPowerOk = IsPowerOk();
	if (bPowerOk && !bWasPowerOk) ScheduleNextTrip();
	bWasPowerOk = bPowerOk;

	if (!bRandomTrips || !bPowerOk) return;

	// Sadece gün sürerken sayar
	const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	if (GM && !GM->IsDayActive()) return;

	TripTimer -= DeltaSeconds;
	if (TripTimer <= 0.f)
	{
		// Jeneratör bozuksa / yeni bozulduysa şimdi değil; arıza aralığı dolunca biraz sonra dene
		float Wait = 0.f;
		UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
		if (Power && !Power->CanStartFailure(Wait))
		{
			TripTimer = Wait + FMath::FRandRange(5.f, 20.f);
			UE_LOG(LogTemp, Log, TEXT("NLFuseBox: sabotage postponed %.0f s (generator failure active/recent)"), TripTimer);
		}
		else
		{
			// Sabotajcı gelip sigortaları karıştırır; gelemezse hemen karıştır
			TripTimer = 1.0e9f;
			TWeakObjectPtr<ANLFuseBox> WeakThis(this);
			if (!ANLSaboteur::RequestSabotage(GetWorld(), this, [WeakThis]() { if (WeakThis.IsValid()) WeakThis->Sabotage(); }))
			{
				Sabotage();
			}
		}
	}
}

void ANLFuseBox::ScheduleNextTrip()
{
	TripTimer = FMath::FRandRange(MinTripInterval, FMath::Max(MinTripInterval, MaxTripInterval));
}

// ------------------------------------------------------------------ Kutunun üstünde fareyle etkileşim

static bool RayHitsBox(const FBox& Box, const FVector& Origin, const FVector& Dir)
{
	return FMath::LineBoxIntersection(Box, Origin, Origin + Dir * 1000.f, Dir * 1000.f);
}

int32 ANLFuseBox::RayPickFuse(const FVector& Origin, const FVector& Dir) const
{
	for (int32 i = 0; i < FuseBodies.Num(); ++i)
	{
		if (!FuseBodies[i]) continue;
		// Gövde + başlıklar, biraz pay ile
		FBox B = FuseBodies[i]->Bounds.GetBox();
		B = B.ExpandBy(FVector(3.f, 3.f, 12.f));
		if (RayHitsBox(B, Origin, Dir)) return i;
	}
	return INDEX_NONE;
}

bool ANLFuseBox::RayToPanel(const FVector& Origin, const FVector& Dir, FVector& OutLocal) const
{
	// Panelin ön yüz düzlemi (kutu yerelinde y = 105)
	const FTransform& T = BoxMesh->GetComponentTransform();
	const FVector P0 = T.TransformPosition(FVector(0.f, 105.f, 0.f));
	const FVector N = T.TransformVectorNoScale(FVector(0.f, 1.f, 0.f));
	const float Den = FVector::DotProduct(Dir, N);
	if (FMath::IsNearlyZero(Den)) return false;
	const float Dist = FVector::DotProduct(P0 - Origin, N) / Den;
	if (Dist <= 0.f) return false;
	OutLocal = T.InverseTransformPosition(Origin + Dir * Dist);
	return true;
}

int32 ANLFuseBox::RayPickSlot(const FVector& Origin, const FVector& Dir) const
{
	FVector L;
	if (!RayToPanel(Origin, Dir, L)) return INDEX_NONE;
	int32 Best = INDEX_NONE;
	float BestD = 16.f;  // yuva merkezine en fazla bu kadar uzak (cm)
	for (int32 i = 0; i < NumFuses; ++i)
	{
		const FVector S = NLFuseLayout::SlotPos[i];
		const float D = FVector2D(L.X - S.X, (L.Z - S.Z) * 0.6f).Size();   // dikey sigorta: dikeyde daha toleranslı
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

bool ANLFuseBox::RayHitsLever(const FVector& Origin, const FVector& Dir) const
{
	return LeverMesh && RayHitsBox(LeverMesh->Bounds.GetBox().ExpandBy(5.f), Origin, Dir);
}

void ANLFuseBox::PlaceFuseMeshes(int32 SlotIdx, const FVector& LocalPos, float Scale)
{
	if (!FuseBodies.IsValidIndex(SlotIdx)) return;
	FuseBodies[SlotIdx]->SetRelativeLocation(LocalPos);
	FuseBodies[SlotIdx]->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.2f) * Scale);
	for (int32 c = 0; c < 2; ++c)
	{
		const int32 CapIdx = SlotIdx * 2 + c;
		if (!FuseCaps.IsValidIndex(CapIdx)) continue;
		FuseCaps[CapIdx]->SetRelativeLocation(LocalPos + FVector(0.f, 0.f, (c == 0 ? 10.f : -10.f) * Scale));
		FuseCaps[CapIdx]->SetRelativeScale3D(FVector(0.07f, 0.07f, 0.045f) * Scale);
	}
}

void ANLFuseBox::ResetFuseMeshes()
{
	for (int32 i = 0; i < NumFuses; ++i)
	{
		const bool bHover = i == HoveredSlot && DraggedSlot == INDEX_NONE;
		PlaceFuseMeshes(i, NLFuseLayout::SlotPos[i] + FVector(0.f, bHover ? 2.f : 0.f, 0.f), bHover ? 1.12f : 1.f);
	}
}

void ANLFuseBox::BeginDrag(int32 SlotIdx)
{
	if (!SlotFuse.IsValidIndex(SlotIdx)) return;
	DraggedSlot = SlotIdx;
	UGameplayStatics::PlaySoundAtLocation(this, FuseMoveSound, GetActorLocation(), 0.6f, 1.3f);
}

void ANLFuseBox::UpdateDrag(const FVector& Origin, const FVector& Dir)
{
	if (DraggedSlot == INDEX_NONE) return;
	FVector L;
	if (!RayToPanel(Origin, Dir, L)) return;
	// Panelin önünde, fareyi takip eder (yuvadan çekilmiş gibi biraz öne çıkık)
	L.Y = 112.f;
	L.X = FMath::Clamp(L.X, -88.f, 98.f);
	L.Z = FMath::Clamp(L.Z, 6.f, 96.f);
	PlaceFuseMeshes(DraggedSlot, L, 1.08f);
}

void ANLFuseBox::EndDrag()
{
	DraggedSlot = INDEX_NONE;
	ResetFuseMeshes();
}

void ANLFuseBox::SetHoveredSlot(int32 SlotIdx)
{
	if (HoveredSlot == SlotIdx) return;
	HoveredSlot = SlotIdx;
	if (DraggedSlot == INDEX_NONE) ResetFuseMeshes();
}

FVector ANLFuseBox::GetSlotWorldLocation(int32 SlotIdx) const
{
	return BoxMesh->GetComponentTransform().TransformPosition(NLFuseLayout::SlotPos[FMath::Clamp(SlotIdx, 0, NumFuses - 1)]);
}

void ANLFuseBox::SetViewing(bool bViewing)
{
	if (ViewLight) ViewLight->SetIntensity(bViewing ? 900.f : 0.f);
	if (!bViewing) { HoveredSlot = INDEX_NONE; EndDrag(); }
}

void ANLFuseBox::SetLeverDragAngle(float AngleDeg)
{
	bLeverDragging = true;
	const float Lo = FMath::Min(LeverOnAngle, LeverOffAngle);
	const float Hi = FMath::Max(LeverOnAngle, LeverOffAngle);
	LeverAngle = FMath::Clamp(AngleDeg, Lo, Hi);
	PlaceLever(LeverAngle);
}

void ANLFuseBox::EndLeverDrag()
{
	if (!bLeverDragging) return;
	bLeverDragging = false;
	// Yarıyı geçtiyse o tarafa geç; geçmediyse kol eski yerine döner (Tick)
	const float Mid = (LeverOnAngle + LeverOffAngle) * 0.5f;
	const bool bTowardOn = (LeverOnAngle > LeverOffAngle) ? (LeverAngle > Mid) : (LeverAngle < Mid);
	SetOn(bTowardOn);
}

FVector ANLFuseBox::GetLeverWorldLocation() const
{
	return LeverMesh ? LeverMesh->Bounds.Origin : GetActorLocation();
}
