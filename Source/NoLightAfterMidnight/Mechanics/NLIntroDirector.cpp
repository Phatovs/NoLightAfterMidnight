#include "NLIntroDirector.h"
#include "NLDefaults.h"
#include "NLGameMode.h"
#include "NLPlayerCharacter.h"
#include "NLWeatherDirector.h"
#include "NLSeatedFigure.h"
#include "NLEnvKit.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace NLIntro
{
	// Bot gövdesi su çizgisinden bu kadar yukarıda biter (oyuncu bunun üstünde durur)
	constexpr float DeckTop = -3.f;   // döşeme tahtalarının üstü
	// Oyuncunun bottan inmeden önce durduğu yer (botun yerel X'i, arkaya doğru)
	constexpr float StandX = -60.f;
}

ANLIntroDirector::ANLIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BoatRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Boat"));
	BoatRoot->SetupAttachment(RootComponent);
	BoatRoot->SetUsingAbsoluteLocation(true);
	BoatRoot->SetUsingAbsoluteRotation(true);
	BoatRoot->SetMobility(EComponentMobility::Movable);

	// Ahşap motorlu sandal (X = ileri, orijin su çizgisinde): kaplama tahtalı gövde, kaburgalar, oturaklar, dıştan takma motor,
	// içinde kürekler, halat, benzin bidonu; pruvada direğe asılı fener. Dümende oturan karakter: ANLSeatedFigure (BeginPlay).
	UStaticMeshComponent* Hull = AddPart(TEXT("Hull"), nullptr, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector);
	NL_KIT_MESH(Hull, "SM_NL_Boat");
	UStaticMeshComponent* Motor = AddPart(TEXT("Motor"), nullptr, FVector(-249, 0, 50), FRotator::ZeroRotator, FVector::OneVector);
	NL_KIT_MESH(Motor, "SM_NL_Outboard");
	UStaticMeshComponent* OarL = AddPart(TEXT("OarL"), nullptr, FVector(55, -42, 33), FRotator(0, 4, 8), FVector::OneVector);
	NL_KIT_MESH(OarL, "SM_NL_Oar");
	UStaticMeshComponent* OarR = AddPart(TEXT("OarR"), nullptr, FVector(45, 40, 33), FRotator(0, -3, -6), FVector::OneVector);
	NL_KIT_MESH(OarR, "SM_NL_Oar");
	UStaticMeshComponent* Pole = AddPart(TEXT("LanternPole"), nullptr, FVector(200, 0, 74), FRotator::ZeroRotator, FVector(0.06f, 0.3f, 1.55f));
	NL_KIT_MESH(Pole, "SM_NL_Post");
	Pole->SetRelativeLocation(FVector(200, 0, -3));
	Pole->SetRelativeScale3D(FVector(0.3f, 0.3f, 1.55f));
	UStaticMeshComponent* Arm = AddPart(TEXT("LanternArm"), nullptr, FVector(212, 0, 150), FRotator::ZeroRotator, FVector(0.32f, 0.25f, 1.2f));
	NL_KIT_MESH(Arm, "SM_NL_Board");
	UStaticMeshComponent* Lamp = AddPart(TEXT("Lantern"), nullptr, FVector(224, 0, 148), FRotator::ZeroRotator, FVector::OneVector);
	NL_KIT_MESH(Lamp, "SM_NL_Lantern");
	UStaticMeshComponent* Rope = AddPart(TEXT("Rope"), nullptr, FVector(165, -18, -4), FRotator(0, 30, 0), FVector(0.6f));
	NL_KIT_MESH(Rope, "SM_NL_RopeCoil");
	UStaticMeshComponent* Can = AddPart(TEXT("Can"), nullptr, FVector(-110, 46, -4), FRotator(0, 80, 0), FVector(0.9f));
	NL_KIT_MESH(Can, "SM_NL_JerryCan");

	BoatLantern = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoatLantern"));
	BoatLantern->SetupAttachment(BoatRoot);
	BoatLantern->SetRelativeLocation(FVector(224, 0, 122));
	BoatLantern->SetIntensityUnits(ELightUnits::Candelas);
	BoatLantern->SetIntensity(10.f);
	BoatLantern->SetAttenuationRadius(1100.f);
	BoatLantern->SetLightColor(FLinearColor(1.f, 0.66f, 0.35f));
	BoatLantern->SetCastShadows(false);

	CineCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CineCamera"));
	CineCamera->SetupAttachment(RootComponent);
	CineCamera->bConstrainAspectRatio = true;   // sinema bantları
	CineCamera->AspectRatio = 2.35f;
	CineCamera->SetFieldOfView(60.f);

	NL_DEFAULT_SOUND(EngineLoop, "boat_engine_loop");
	NL_DEFAULT_SOUND(EngineStopSound, "boat_engine_stop");
	NL_DEFAULT_SOUND(DockBumpSound, "boat_dock_bump");
	NL_DEFAULT_SOUND(StepOffSound, "boat_step_off");
}

void ANLIntroDirector::SetPilotVisible(bool bVisible)
{
	if (Figure) Figure->SetActorHiddenInGame(!bVisible);
}

UStaticMeshComponent* ANLIntroDirector::AddPart(const TCHAR* Name, UStaticMesh* Mesh, FVector Loc, FRotator Rot, FVector Scale)
{
	UStaticMeshComponent* P = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	P->SetupAttachment(BoatRoot);
	if (Mesh) P->SetStaticMesh(Mesh);
	P->SetRelativeLocation(Loc);
	P->SetRelativeRotation(Rot);
	P->SetRelativeScale3D(Scale);
	P->SetMobility(EComponentMobility::Movable);
	// Sadece görüş izine takılır (yağmur damlaları gövdede durur), oyuncuyu itmez
	P->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	P->SetCollisionResponseToAllChannels(ECR_Ignore);
	P->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	P->SetCanEverAffectNavigation(false);
	BoatParts.Add(P);
	return P;
}

void ANLIntroDirector::ColorParts()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Base) return;
	auto Paint = [this, Base](UStaticMeshComponent* P, const FLinearColor& Col)
	{
		if (!P) return;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
		M->SetVectorParameterValue(TEXT("Color"), Col);
		P->SetMaterial(0, M);
	};
	for (UStaticMeshComponent* P : BoatParts)
	{
		const FString N = P->GetName();
		FLinearColor Col(0.07f, 0.02f, 0.015f);                                          // gövde: koyu kırmızı boya
		if (N == TEXT("Floor")) Col = FLinearColor(0.035f, 0.028f, 0.02f);
		else if (N.StartsWith(TEXT("Rail")) || N.StartsWith(TEXT("Seat")) || N == TEXT("Tiller")) Col = FLinearColor(0.08f, 0.055f, 0.035f);
		else if (N.StartsWith(TEXT("Motor")) || N == TEXT("LanternPole")) Col = FLinearColor(0.04f, 0.04f, 0.045f);
		else if (N == TEXT("Lantern")) Col = FLinearColor(1.f, 0.75f, 0.4f);
		else if (N == TEXT("Pilot")) Col = FLinearColor(0.1f, 0.085f, 0.03f);               // sarımsı yağmurluk
		else if (N == TEXT("PilotHead")) Col = FLinearColor(0.08f, 0.068f, 0.025f);           // kapüşon
		Paint(P, Col);
	}
}

void ANLIntroDirector::BeginPlay()
{
	Super::BeginPlay();
	// Dümende oturan karakter (tekneyle birlikte sallanır)
	FActorSpawnParameters SP;
	SP.Owner = this;
	Figure = GetWorld()->SpawnActor<ANLSeatedFigure>(ANLSeatedFigure::StaticClass(), BoatRoot->GetComponentTransform(), SP);
	if (Figure)
	{
		Figure->AttachToComponent(BoatRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Figure->SetActorRelativeLocation(FVector(-190.f, 0.f, 28.f));
		Figure->FloorDrop = 31.f;
		Figure->RightHandTarget = FVector(24.f, 32.f, 42.f);
	}

	BoatAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = BoatAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
		A.FalloffDistance = 6000.f;
	}
	ComputePoints();
	// Varsayılan: bot rıhtımda bağlı (GameMode Play() ya da PlaceDocked() çağırır)
	BoatPos = Moor;
	BoatYaw = (-Dir).Rotation().Yaw;
	SetPilotVisible(false);
	ApplyBoatTransform();
}

void ANLIntroDirector::ComputePoints()
{
	Dir = FRotator(0.f, DockYaw, 0.f).Vector();
	const FVector N(-Dir.Y, Dir.X, 0.f);
	const float Off = DockHalfWidth + 110.f;
	auto MoorFor = [&](float S) { return FVector(DockCenter.X, DockCenter.Y, WaterZ) + Dir * MooringAlong + N * (S * Off); };
	auto GroundZ = [this](const FVector& P)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NLIntroGround), false, this);
		return GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 3000.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q) ? Hit.ImpactPoint.Z : -3000.f;
	};

	float S = BoatSide >= 0 ? 1.f : -1.f;
	if (BoatSide == 0)
	{
		// Suyu derin olan (zemini alçak) tarafa yanaş. Not: deniz yüzeyi de iz alır, o yüzden suyun hemen altından bak
		auto Depth = [&](float Sg)
		{
			// Deniz ince bir kutu: suyun hemen altından aşağı bakıp altındaki araziyi bul; su üstünde kara varsa çok sığ say
			const FVector P = MoorFor(Sg);
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NLIntroDepth), false, this);
			float Ground = -3000.f;
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, WaterZ - 40.f), FVector(P.X, P.Y, -3000.f), ECC_Visibility, Q)) Ground = Hit.ImpactPoint.Z;
			const float Top = GroundZ(P);
			return Top > WaterZ + 20.f ? Top : Ground;
		};
		const float A = Depth(1.f), B = Depth(-1.f);
		S = A <= B ? 1.f : -1.f;
		UE_LOG(LogTemp, Log, TEXT("NLIntro: ground under mooring spots: side+1 %.0f, side-1 %.0f -> side %+.0f"), A, B, S);
	}
	Side = N * S;
	Moor = MoorFor(S);

	// Yaklaşma eğrisi: denizden, yana açık bir kavisle gelip rıhtıma paralel yanaşır
	P2 = Moor + Dir * 1800.f;
	P1 = Moor + Dir * 3800.f + Side * 1500.f;
	P0 = Moor + Dir * 5600.f + Side * 2700.f;

	StepFrom = Moor + FVector(0, 0, NLIntro::DeckTop) + (-Dir) * NLIntro::StandX;
	StepTo = FVector(DockCenter.X, DockCenter.Y, DockCenter.Z) + Dir * (MooringAlong - NLIntro::StandX) + Side * (DockHalfWidth - 90.f);
	StepYawFrom = (-Side).Rotation().Yaw;
	StepYawTo = (-Dir).Rotation().Yaw;
	bComputed = true;
}

FVector ANLIntroDirector::PathAt(float U, FVector* OutTangent) const
{
	const float V = 1.f - U;
	if (OutTangent)
	{
		*OutTangent = 3.f * V * V * (P1 - P0) + 6.f * V * U * (P2 - P1) + 3.f * U * U * (Moor - P2);
	}
	return V * V * V * P0 + 3.f * V * V * U * P1 + 3.f * V * U * U * P2 + U * U * U * Moor;
}

void ANLIntroDirector::ApplyBoatTransform()
{
	// Dalgada sallanma (+ rıhtıma çarpınca yana yatma)
	const float T = BobTime;
	const float Z = 4.f * FMath::Sin(T * 1.4f) + 1.5f * FMath::Sin(T * 2.3f + 0.7f);
	const float Pitch = 1.8f * FMath::Sin(T * 0.9f) + 0.8f * FMath::Sin(T * 1.9f);
	const float Roll = 2.4f * FMath::Sin(T * 1.15f + 1.f) + 5.f * BumpKick * FMath::Sin(T * 7.f);
	BoatRoot->SetWorldLocationAndRotation(BoatPos + FVector(0, 0, Z), FRotator(Pitch, BoatYaw, Roll));
}

void ANLIntroDirector::Play()
{
	ANLPlayerCharacter* P = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!P || !PC) { Finish(true); return; }
	Player = P;
	if (!bComputed) ComputePoints();

	bPlaying = true;
	Time = 0.f;
	Shot = -1;
	SetPilotVisible(true);

	// Oyuncu sinematik boyunca gizli, kontrolsüz; rıhtımda bekler
	P->SetCinematicMode(true);
	P->SetActorHiddenInGame(true);
	P->GetCharacterMovement()->DisableMovement();
	P->SetActorLocation(StepTo + FVector(0, 0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);

	EngineAudio = UGameplayStatics::SpawnSoundAttached(EngineLoop, BoatRoot, NAME_None, FVector(-250, 0, 60), EAttachLocation::KeepRelativeOffset,
		false, 1.f, 1.f, 0.f, BoatAttenuation, nullptr, false);

	SetShot(0);
	PC->SetViewTarget(this);
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.5f, FLinearColor::Black, false, false);
	UE_LOG(LogTemp, Log, TEXT("NLIntro: boat intro started (moor at %.0f,%.0f, %.0f m of path)"), Moor.X, Moor.Y, FVector::Dist(P0, Moor) / 100.f);
}

void ANLIntroDirector::PlaceDocked(bool bMovePlayer)
{
	if (!bComputed) ComputePoints();
	bPlaying = false;
	BoatPos = Moor;
	BoatYaw = (-Dir).Rotation().Yaw;
	SetPilotVisible(false);
	ApplyBoatTransform();
	if (!bMovePlayer) return;
	if (ANLPlayerCharacter* P = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		P->SetActorLocation(StepTo + FVector(0, 0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (P->GetController()) P->GetController()->SetControlRotation(FRotator(0.f, StepYawTo, 0.f));
		UE_LOG(LogTemp, Log, TEXT("NLIntro: restart - player starts on the dock"));
	}
}

void ANLIntroDirector::SetShot(int32 NewShot)
{
	if (NewShot == Shot) return;
	Shot = NewShot;
	const FVector Up(0, 0, 1);
	switch (Shot)
	{
	case 0:   // denizden geniş plan, botu takip eder
		CineCamera->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		CineCamera->SetWorldLocation(Moor + Dir * 3200.f - Side * 900.f + Up * 150.f);
		CineCamera->SetFieldOfView(50.f);
		break;
	case 1:   // botun arkasından, dümendekinin omzunun üstünden
		CineCamera->AttachToComponent(BoatRoot, FAttachmentTransformRules::KeepRelativeTransform);
		CineCamera->SetRelativeLocationAndRotation(FVector(-430.f, 45.f, 215.f), FRotator(-9.f, -3.f, 0.f));
		CineCamera->SetFieldOfView(72.f);
		break;
	case 2:   // rıhtımın öbür kenarından, gelip yanaşan bota çapraz bakar (kulübeye girmesin diye karşı taraf)
		CineCamera->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		CineCamera->SetWorldLocation(DockCenter + Dir * (MooringAlong - 750.f) - Side * (DockHalfWidth - 40.f) + Up * 175.f);
		CineCamera->SetFieldOfView(50.f);
		break;
	default: break;
	}
	// İlk karede doğrudan bota baksın (sonra yumuşak takip)
	if (Shot != 1)
	{
		CineCamera->SetWorldRotation((BoatRoot->GetComponentLocation() + FVector(0, 0, 60.f) - CineCamera->GetComponentLocation()).Rotation());
	}
	UE_LOG(LogTemp, Log, TEXT("NLIntro: shot %d at %.1f s"), Shot, Time);
}

void ANLIntroDirector::UpdateCamera(float DeltaSeconds)
{
	if (Shot == 0 || Shot == 2)
	{
		const FVector Target = BoatRoot->GetComponentLocation() + FVector(0, 0, Shot == 0 ? 80.f : 50.f);
		const FRotator Want = (Target - CineCamera->GetComponentLocation()).Rotation();
		CineCamera->SetWorldRotation(FMath::RInterpTo(CineCamera->GetComponentRotation(), Want, DeltaSeconds, 2.2f));
		if (Shot == 0)
		{
			// Çok yavaş ileri kayma
			CineCamera->AddWorldOffset(-Dir * 40.f * DeltaSeconds);
		}
	}
}

void ANLIntroDirector::UpdateBoat(float DeltaSeconds)
{
	const float S = FMath::Clamp(Time / FMath::Max(ArriveTime, 1.f), 0.f, 1.f);
	const float U = 1.f - FMath::Square(1.f - S);   // yavaşlayarak yanaşır
	FVector Tangent;
	BoatPos = PathAt(U, &Tangent);
	if (Tangent.SizeSquared2D() > 1.f && S < 0.995f)
	{
		BoatYaw = FMath::FInterpTo(BoatYaw, BoatYaw + FMath::FindDeltaAngleDegrees(BoatYaw, Tangent.Rotation().Yaw), DeltaSeconds, 3.f);
	}

	if (EngineAudio)
	{
		// Yanaşırken gaz kesilir
		EngineAudio->SetPitchMultiplier(FMath::Lerp(1.05f, 0.75f, FMath::Clamp((S - 0.6f) / 0.4f, 0.f, 1.f)));
		EngineAudio->SetVolumeMultiplier(FMath::Lerp(1.f, 0.55f, FMath::Clamp((S - 0.6f) / 0.4f, 0.f, 1.f)));
	}

	if (!bBumped && S >= 1.f)
	{
		bBumped = true;
		BumpKick = 1.f;
		UGameplayStatics::PlaySoundAtLocation(this, DockBumpSound, BoatPos + Side * -90.f, FRotator::ZeroRotator, 1.f, 1.f, 0.f, BoatAttenuation);
		if (EngineAudio) { EngineAudio->FadeOut(0.4f, 0.f); EngineAudio = nullptr; }
		UGameplayStatics::PlaySoundAtLocation(this, EngineStopSound, BoatRoot->GetComponentTransform().TransformPosition(FVector(-250, 0, 60)),
			FRotator::ZeroRotator, 0.8f, 1.f, 0.f, BoatAttenuation);
		UE_LOG(LogTemp, Log, TEXT("NLIntro: boat docked, bump"));
	}
}

void ANLIntroDirector::StartFirstPerson()
{
	bFirstPerson = true;
	ANLPlayerCharacter* P = Player.Get();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!P || !PC) { Finish(true); return; }

	SetPilotVisible(false);
	const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	P->SetActorHiddenInGame(false);
	P->SetActorLocation(StepFrom + FVector(0, 0, HH), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(FRotator(-14.f, StepYawFrom, 0.f));
	PC->SetViewTarget(P);
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.9f, FLinearColor::Black, false, false);
	UE_LOG(LogTemp, Log, TEXT("NLIntro: first person on the boat"));
}

void ANLIntroDirector::Finish(bool bSkipped)
{
	if (bFinished) return;
	bFinished = true;
	bPlaying = false;
	if (EngineAudio) { EngineAudio->FadeOut(0.3f, 0.f); EngineAudio = nullptr; }
	BumpKick = 0.f;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ANLPlayerCharacter* P = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (bSkipped)
	{
		PlaceDocked(true);
		if (PC && PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.f, FLinearColor::Black, false, false);
	}
	if (P)
	{
		P->SetActorHiddenInGame(false);
		P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		P->SetCinematicMode(false);
	}
	if (PC && P) PC->SetViewTarget(P);
	UE_LOG(LogTemp, Log, TEXT("NLIntro: finished%s"), bSkipped ? TEXT(" (skipped)") : TEXT(""));

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->FinishIntro();
}

void ANLIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BobTime += DeltaSeconds;
	BumpKick = FMath::FInterpTo(BumpKick, 0.f, DeltaSeconds, 1.2f);

	if (!bPlaying)
	{
		ApplyBoatTransform();
		return;
	}

	Time += DeltaSeconds;

	// Geç: boşluk / Enter / Esc
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (Time > 0.5f && (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::Escape)))
		{
			Finish(true);
			ApplyBoatTransform();
			return;
		}
	}

	UpdateBoat(DeltaSeconds);
	ApplyBoatTransform();

	if (!bFirstPerson)
	{
		if (Time >= ShotCTime) SetShot(2);
		else if (Time >= ShotBTime) SetShot(1);
		UpdateCamera(DeltaSeconds);

		// İlk planda şimşek
		if (!bLightningDone && Time >= 3.2f)
		{
			bLightningDone = true;
			for (TActorIterator<ANLWeatherDirector> It(GetWorld()); It; ++It) { if (It->IsRaining()) It->TriggerLightning(); }
		}

		// Yanaştıktan sonra kararma, birinci şahsa geçiş
		const float FadeAt = ArriveTime + 1.4f;
		if (Time >= FadeAt && Time - DeltaSeconds < FadeAt)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.6f, FLinearColor::Black, false, true);
			}
		}
		if (Time >= FadeAt + 0.7f) StartFirstPerson();
		return;
	}

	// Birinci şahıs: bottan rıhtıma adım, sonra adaya dön
	ANLPlayerCharacter* P = Player.Get();
	AController* C = P ? P->GetController() : nullptr;
	if (!P || !C) { Finish(true); return; }
	const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float StepStart = ArriveTime + 2.1f + 0.8f;
	const float A = FMath::Clamp((Time - StepStart) / FMath::Max(StepOffDuration, 0.1f), 0.f, 1.f);
	const float SA = FMath::SmoothStep(0.f, 1.f, A);

	// Bot sallandıkça oyuncu da üstünde sallanır (inmeden önce)
	const FVector OnBoat = BoatRoot->GetComponentTransform().TransformPosition(FVector(NLIntro::StandX, 0.f, NLIntro::DeckTop)) + FVector(0, 0, HH);
	const FVector OnDock = StepTo + FVector(0, 0, HH + 2.f);
	FVector Pos = FMath::Lerp(OnBoat, OnDock, SA);
	Pos.Z += 38.f * FMath::Sin(PI * SA);
	P->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);

	if (A > 0.f && !bStepStarted) { bStepStarted = true; UE_LOG(LogTemp, Log, TEXT("NLIntro: stepping off the boat")); }
	if (A >= 0.9f && !bLanded)
	{
		bLanded = true;
		UGameplayStatics::PlaySoundAtLocation(this, StepOffSound, OnDock - FVector(0, 0, HH));
	}

	// Bakış: önce rıhtıma, indikten sonra adaya doğru
	const float TurnA = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Time - StepStart - StepOffDuration * 0.6f) / 1.6f, 0.f, 1.f));
	const float Yaw = StepYawFrom + FMath::FindDeltaAngleDegrees(StepYawFrom, StepYawTo) * TurnA;
	const float Pitch = FMath::Lerp(-14.f, -2.f, FMath::Max(SA, TurnA));
	C->SetControlRotation(FRotator(Pitch, Yaw, 0.f));

	if (Time >= StepStart + StepOffDuration + 1.9f) Finish(false);
}
