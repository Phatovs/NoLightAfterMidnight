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
#include "Materials/MaterialInterface.h"
#include "Sound/SoundAttenuation.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace NLIntro
{
	// SM_NL_Trawler ölçüleri (Scripts/blender_trawler.py)
	constexpr float DeckTop = 80.f;          // güverte üstü (su çizgisinden)
	constexpr float HalfBeam = 175.f;        // en geniş yerin yarısı
	constexpr float StandX = -150.f;         // oyuncunun arka güvertedeki yeri (yerel X)
	const FVector WheelCenter(112.f, 0.f, 175.f);
	constexpr float WheelRadius = 22.f;
	const FVector CaptainSpot(84.f, 0.f, DeckTop);
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

	// Eski balıkçı teknesi (X = pruva, orijin su çizgisinde): gövde, köşk, direk, bumba, güverte eşyaları tek mesh.
	// Dümen ayrı mesh (kaptan çevirir). Kaptan ve yolcu: ANLSeatedFigure (BeginPlay).
	UStaticMeshComponent* Hull = AddPart(TEXT("Hull"), nullptr, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector);
	NL_KIT_MESH(Hull, "SM_NL_Trawler");
	Wheel = AddPart(TEXT("Wheel"), nullptr, NLIntro::WheelCenter, FRotator::ZeroRotator, FVector::OneVector);
	NL_KIT_MESH(Wheel, "SM_NL_TrawlerWheel");
	UStaticMeshComponent* Can = AddPart(TEXT("Can"), nullptr, FVector(-300.f, -105.f, NLIntro::DeckTop), FRotator(0, 70, 0), FVector::OneVector);
	NL_KIT_MESH(Can, "SM_NL_JerryCan");
	UStaticMeshComponent* Can2 = AddPart(TEXT("Can2"), nullptr, FVector(-322.f, -88.f, NLIntro::DeckTop), FRotator(0, 95, 0), FVector::OneVector);
	NL_KIT_MESH(Can2, "SM_NL_JerryCan");

	BoatLantern = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoatLantern"));
	BoatLantern->SetupAttachment(BoatRoot);
	BoatLantern->SetRelativeLocation(FVector(-150.f, 0.f, 330.f));
	BoatLantern->SetIntensityUnits(ELightUnits::Candelas);
	BoatLantern->SetIntensity(14.f);
	BoatLantern->SetAttenuationRadius(1300.f);
	BoatLantern->SetLightColor(FLinearColor(1.f, 0.7f, 0.42f));
	BoatLantern->SetCastShadows(false);

	CabinLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CabinLight"));
	CabinLight->SetupAttachment(BoatRoot);
	CabinLight->SetRelativeLocation(FVector(50.f, 0.f, 268.f));
	CabinLight->SetIntensityUnits(ELightUnits::Candelas);
	CabinLight->SetIntensity(5.f);
	CabinLight->SetAttenuationRadius(420.f);
	CabinLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.45f));
	CabinLight->SetCastShadows(false);

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
	// Hareketli: uzaklık alanı / ray tracing güncellemesi her kare maliyet çıkarmasın
	P->bAffectDistanceFieldLighting = false;
	P->bVisibleInRayTracing = false;
	BoatParts.Add(P);
	return P;
}

void ANLIntroDirector::SetBoatVisible(bool bVisible)
{
	BoatRoot->SetVisibility(bVisible, true);
	if (Captain) Captain->SetActorHiddenInGame(!bVisible);
	if (Companion) Companion->SetActorHiddenInGame(!bVisible);
}

void ANLIntroDirector::SpawnCrew()
{
	FActorSpawnParameters SP;
	SP.Owner = this;
	UMaterialInterface* Raincoat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/MI_NL_Raincoat.MI_NL_Raincoat"));
	UMaterialInterface* Coat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Materials/MI_NL_DarkCoat.MI_NL_DarkCoat"));

	// Kaptan: köşkte ayakta, iki eli dümende, pruvaya bakar
	Captain = GetWorld()->SpawnActorDeferred<ANLSeatedFigure>(ANLSeatedFigure::StaticClass(), BoatRoot->GetComponentTransform(), this);
	if (Captain)
	{
		Captain->bSeated = false;
		Captain->Clothing = Raincoat;
		Captain->LookRotation = FRotator(-4.f, 0.f, 0.f);
		UGameplayStatics::FinishSpawningActor(Captain, BoatRoot->GetComponentTransform());
		Captain->AttachToComponent(BoatRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Captain->SetActorRelativeLocation(NLIntro::CaptainSpot + FVector(0.f, 0.f, Captain->StandHipHeight));
	}
	// Yolcu: arka güvertede oyuncunun yanında, rıhtım tarafına bakar; bir eli küpeştede
	Companion = GetWorld()->SpawnActorDeferred<ANLSeatedFigure>(ANLSeatedFigure::StaticClass(), BoatRoot->GetComponentTransform(), this);
	if (Companion)
	{
		Companion->bSeated = false;
		Companion->Clothing = Coat;
		UGameplayStatics::FinishSpawningActor(Companion, BoatRoot->GetComponentTransform());
		Companion->AttachToComponent(BoatRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Companion->SetActorRelativeLocation(FVector(NLIntro::StandX - 125.f, StandY * 0.9f, NLIntro::DeckTop + Companion->StandHipHeight));
		Companion->SetActorRelativeRotation(FRotator(0.f, StandY > 0.f ? 75.f : -75.f, 0.f));
	}
}

void ANLIntroDirector::BeginPlay()
{
	Super::BeginPlay();
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
	SpawnCrew();
	// Varsayılan: tekne rıhtımda (GameMode Play() ya da PlaceDocked() çağırır)
	BoatPos = Moor;
	BoatYaw = (-Dir).Rotation().Yaw;
	LastYaw = BoatYaw;
	ApplyBoatTransform();
}

void ANLIntroDirector::ComputePoints()
{
	Dir = FRotator(0.f, DockYaw, 0.f).Vector();
	const FVector N(-Dir.Y, Dir.X, 0.f);
	// Tekne rıhtıma bordasıyla değer (yarım en + usturmaça payı)
	const float Off = DockHalfWidth + NLIntro::HalfBeam + 22.f;
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

	// Oyuncu teknede rıhtım tarafında durur: teknenin yerel Y'si rıhtıma bakan yön
	const FVector BoatRight = FRotationMatrix(FRotator(0.f, (-Dir).Rotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	StandY = (FVector::DotProduct(-Side, BoatRight) >= 0.f ? 1.f : -1.f) * 108.f;

	StepFrom = Moor + FVector(0, 0, NLIntro::DeckTop) + (-Dir) * NLIntro::StandX - Side * FMath::Abs(StandY);
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
	// Dalgada sallanma (+ rıhtıma çarpınca yana yatma). Büyük tekne: daha ağır, yavaş salınım
	const float T = BobTime;
	const float Z = 3.5f * FMath::Sin(T * 1.1f) + 1.2f * FMath::Sin(T * 2.0f + 0.7f);
	const float Pitch = 1.2f * FMath::Sin(T * 0.8f) + 0.5f * FMath::Sin(T * 1.7f);
	const float Roll = 2.0f * FMath::Sin(T * 0.95f + 1.f) + 3.5f * BumpKick * FMath::Sin(T * 6.f);
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
	SetBoatVisible(true);

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
	UE_LOG(LogTemp, Log, TEXT("NLIntro: trawler intro started (moor at %.0f,%.0f, %.0f m of path)"), Moor.X, Moor.Y, FVector::Dist(P0, Moor) / 100.f);
}

void ANLIntroDirector::MovePlayerToDock()
{
	if (ANLPlayerCharacter* P = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		P->SetActorLocation(StepTo + FVector(0, 0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (P->GetController()) P->GetController()->SetControlRotation(FRotator(0.f, StepYawTo, 0.f));
	}
}

void ANLIntroDirector::PlaceDocked(bool bMovePlayer)
{
	if (!bComputed) ComputePoints();
	bPlaying = false;
	// Tekne oyuncuyu bırakıp çoktan gitti
	bGone = true;
	bDeparting = false;
	SetBoatVisible(false);
	if (!bMovePlayer) return;
	MovePlayerToDock();
	UE_LOG(LogTemp, Log, TEXT("NLIntro: restart - player starts on the dock (no boat)"));
}

void ANLIntroDirector::SetShot(int32 NewShot)
{
	if (NewShot == Shot) return;
	Shot = NewShot;
	const FVector Up(0, 0, 1);
	switch (Shot)
	{
	case 0:   // denizden geniş plan, tekneyi takip eder
		CineCamera->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		CineCamera->SetWorldLocation(Moor + Dir * 3200.f - Side * 1100.f + Up * 200.f);
		CineCamera->SetFieldOfView(50.f);
		break;
	case 1:   // köşkün içinden, kaptanın arkasından: dümen, camlar, karşıda ada
		CineCamera->AttachToComponent(BoatRoot, FAttachmentTransformRules::KeepRelativeTransform);
		CineCamera->SetRelativeLocationAndRotation(FVector(-28.f, -72.f, 248.f), FRotator(-10.f, 14.f, 0.f));
		CineCamera->SetFieldOfView(84.f);
		break;
	case 2:   // rıhtımın öbür kenarından, gelip yanaşan tekneye çapraz bakar
		CineCamera->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
		CineCamera->SetWorldLocation(DockCenter + Dir * (MooringAlong - 1050.f) - Side * (DockHalfWidth - 40.f) + Up * 190.f);
		CineCamera->SetFieldOfView(52.f);
		break;
	default: break;
	}
	// İlk karede doğrudan tekneye baksın (sonra yumuşak takip)
	if (Shot != 1)
	{
		CineCamera->SetWorldRotation((BoatRoot->GetComponentLocation() + FVector(0, 0, 150.f) - CineCamera->GetComponentLocation()).Rotation());
	}
	UE_LOG(LogTemp, Log, TEXT("NLIntro: shot %d at %.1f s"), Shot, Time);
}

void ANLIntroDirector::UpdateCamera(float DeltaSeconds)
{
	if (Shot == 0 || Shot == 2)
	{
		const FVector Target = BoatRoot->GetComponentLocation() + FVector(0, 0, Shot == 0 ? 180.f : 140.f);
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
		EngineAudio->SetPitchMultiplier(FMath::Lerp(1.0f, 0.72f, FMath::Clamp((S - 0.6f) / 0.4f, 0.f, 1.f)));
		EngineAudio->SetVolumeMultiplier(FMath::Lerp(1.f, 0.5f, FMath::Clamp((S - 0.6f) / 0.4f, 0.f, 1.f)));
	}

	if (!bBumped && S >= 1.f)
	{
		bBumped = true;
		BumpKick = 1.f;
		UGameplayStatics::PlaySoundAtLocation(this, DockBumpSound, BoatPos + Side * -NLIntro::HalfBeam, FRotator::ZeroRotator, 1.f, 0.8f, 0.f, BoatAttenuation);
		// Motor susmaz, rölantide bekler (yolcuyu bırakıp hemen gidecek)
		if (EngineAudio) { EngineAudio->SetVolumeMultiplier(0.35f); EngineAudio->SetPitchMultiplier(0.62f); }
		UE_LOG(LogTemp, Log, TEXT("NLIntro: boat docked, bump"));
	}
}

void ANLIntroDirector::UpdateCrew(float DeltaSeconds)
{
	if (bGone) return;
	// Dümen: dalgada hafif düzeltmeler + dönüşlerde dönüş yönüne
	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, BoatYaw) / FMath::Max(DeltaSeconds, 0.001f);
	LastYaw = BoatYaw;
	const float Want = FMath::Clamp(YawRate * 9.f, -110.f, 110.f) + 18.f * FMath::Sin(BobTime * 0.6f) + 6.f * FMath::Sin(BobTime * 1.7f);
	WheelAngle = FMath::FInterpTo(WheelAngle, Want, DeltaSeconds, 1.5f);
	if (Wheel) Wheel->SetRelativeRotation(FRotator(0.f, 0.f, WheelAngle));

	if (Captain && Wheel)
	{
		// Eller dümen çemberinde, tepeden ±50 derece; dümenle birlikte döner
		const FTransform W = Wheel->GetComponentTransform();
		auto Rim = [&](float DegFromTop)
		{
			const float A = FMath::DegreesToRadians(DegFromTop);
			return W.TransformPosition(FVector(-3.f, NLIntro::WheelRadius * FMath::Sin(A), NLIntro::WheelRadius * FMath::Cos(A)));
		};
		Captain->SetHandTargetsWorld(1.f, Rim(55.f), 1.f, Rim(-55.f));
	}

	if (Companion)
	{
		// Bir el küpeştede (rıhtım tarafı), diğeri yanda; oyuncu inince el sallar
		const FTransform B = BoatRoot->GetComponentTransform();
		const FVector Rail = B.TransformPosition(FVector(NLIntro::StandX - 110.f, StandY > 0.f ? 152.f : -152.f, 104.f));
		float Wave = 0.f;
		if (LandTime >= 0.f)
		{
			const float T = (bPlaying ? Time : Time + DepartTime) - LandTime;
			Wave = FMath::Clamp(T / 0.5f, 0.f, 1.f) * (1.f - FMath::Clamp((T - 5.5f) / 0.8f, 0.f, 1.f));
		}
		Companion->SetWave(Wave);
		// Sol el küpeştede, sağ el serbest (el sallama için)
		Companion->SetHandTargetsWorld(0.f, Rail, 1.f, Rail);
	}
}

void ANLIntroDirector::StartFirstPerson()
{
	bFirstPerson = true;
	ANLPlayerCharacter* P = Player.Get();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!P || !PC) { Finish(true); return; }

	const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	P->SetActorHiddenInGame(false);
	P->SetActorLocation(StepFrom + FVector(0, 0, HH), false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(FRotator(-10.f, StepYawFrom, 0.f));
	PC->SetViewTarget(P);
	if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.9f, FLinearColor::Black, false, false);
	UE_LOG(LogTemp, Log, TEXT("NLIntro: first person on the trawler"));
}

void ANLIntroDirector::Finish(bool bSkipped)
{
	if (bFinished) return;
	bFinished = true;
	bPlaying = false;
	BumpKick = 0.f;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	ANLPlayerCharacter* P = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (bSkipped)
	{
		// Geçildi: tekne rıhtımda, oyuncu rıhtımda; tekne yine de ayrılır
		BoatPos = Moor;
		BoatYaw = (-Dir).Rotation().Yaw;
		SetBoatVisible(true);
		MovePlayerToDock();
		if (LandTime < 0.f) LandTime = Time;
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
	StartDeparture();

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->FinishIntro();
}

void ANLIntroDirector::StartDeparture()
{
	if (bDeparting || bGone) return;
	bDeparting = true;
	DepartTime = 0.f;
	if (!EngineAudio)
	{
		EngineAudio = UGameplayStatics::SpawnSoundAttached(EngineLoop, BoatRoot, NAME_None, FVector(-250, 0, 60), EAttachLocation::KeepRelativeOffset,
			false, 0.35f, 0.62f, 0.f, BoatAttenuation, nullptr, false);
	}
	UE_LOG(LogTemp, Log, TEXT("NLIntro: trawler departing"));
}

void ANLIntroDirector::UpdateDeparture(float DeltaSeconds)
{
	DepartTime += DeltaSeconds;
	// Yolcu el sallarken biraz bekler, sonra gaz açıp geldiği yoldan denize döner
	const float Wait = 2.5f;
	const float S = FMath::Clamp((DepartTime - Wait) / FMath::Max(DepartDuration, 1.f), 0.f, 1.f);
	if (DepartTime > Wait)
	{
		const float U = 1.f - FMath::Pow(S, 1.4f);   // hızlanarak açılır
		FVector Tangent;
		BoatPos = PathAt(U, &Tangent);
		if (Tangent.SizeSquared2D() > 1.f)
		{
			const float Away = (-Tangent).Rotation().Yaw;
			BoatYaw = FMath::FInterpTo(BoatYaw, BoatYaw + FMath::FindDeltaAngleDegrees(BoatYaw, Away), DeltaSeconds, 0.45f);
		}
		if (EngineAudio)
		{
			const float Gas = FMath::Clamp((DepartTime - Wait) / 3.f, 0.f, 1.f);
			EngineAudio->SetPitchMultiplier(FMath::Lerp(0.62f, 1.0f, Gas));
			EngineAudio->SetVolumeMultiplier(FMath::Lerp(0.35f, 1.f, Gas) * (1.f - FMath::Clamp((S - 0.85f) / 0.15f, 0.f, 1.f)));
		}
	}
	if (S >= 1.f)
	{
		bDeparting = false;
		bGone = true;
		if (EngineAudio) { EngineAudio->Stop(); EngineAudio = nullptr; }
		SetBoatVisible(false);
		UE_LOG(LogTemp, Log, TEXT("NLIntro: trawler gone"));
	}
}

void ANLIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BobTime += DeltaSeconds;
	BumpKick = FMath::FInterpTo(BumpKick, 0.f, DeltaSeconds, 1.2f);
	if (bGone) return;

	if (!bPlaying)
	{
		if (bDeparting) UpdateDeparture(DeltaSeconds);
		ApplyBoatTransform();
		UpdateCrew(DeltaSeconds);
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
	UpdateCrew(DeltaSeconds);

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

	// Birinci şahıs: tekneden rıhtıma adım, dönüp tekneye (yolcu el sallar), sonra adaya
	ANLPlayerCharacter* P = Player.Get();
	AController* C = P ? P->GetController() : nullptr;
	if (!P || !C) { Finish(true); return; }
	const float HH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float StepStart = ArriveTime + 2.1f + 1.4f;   // önce yanındaki yolcuyu görsün
	const float A = FMath::Clamp((Time - StepStart) / FMath::Max(StepOffDuration, 0.1f), 0.f, 1.f);
	const float SA = FMath::SmoothStep(0.f, 1.f, A);

	// Tekne sallandıkça oyuncu da üstünde sallanır (inmeden önce)
	const FVector OnBoat = BoatRoot->GetComponentTransform().TransformPosition(FVector(NLIntro::StandX, StandY, NLIntro::DeckTop)) + FVector(0, 0, HH);
	const FVector OnDock = StepTo + FVector(0, 0, HH + 2.f);
	FVector Pos = FMath::Lerp(OnBoat, OnDock, SA);
	Pos.Z += 30.f * FMath::Sin(PI * SA);
	P->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);

	if (A > 0.f && !bStepStarted) { bStepStarted = true; UE_LOG(LogTemp, Log, TEXT("NLIntro: stepping off the boat")); }
	if (A >= 0.9f && !bLanded)
	{
		bLanded = true;
		LandTime = Time;
		UGameplayStatics::PlaySoundAtLocation(this, StepOffSound, OnDock - FVector(0, 0, HH));
	}

	// Bakış: inmeden önce yanındaki yolcuya kısa bir bakış, inerken rıhtıma, inince dönüp tekneye (el sallayan yolcu), sonra adaya
	float Yaw = StepYawFrom;
	float Pitch = -10.f;
	const float Pre = Time - (ArriveTime + 2.1f);
	if (Companion && Pre < 1.4f)
	{
		const float LookYaw = (Companion->GetActorLocation() + FVector(0, 0, 60.f) - P->GetActorLocation()).Rotation().Yaw;
		const float K = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(Pre / 0.6f, 0.f, 1.f)) * (1.f - FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Pre - 0.8f) / 0.6f, 0.f, 1.f)));
		Yaw = StepYawFrom + FMath::FindDeltaAngleDegrees(StepYawFrom, LookYaw) * K;
	}
	const float After = Time - StepStart - StepOffDuration;
	const float BackYaw = Companion ? (Companion->GetActorLocation() - P->GetActorLocation()).Rotation().Yaw : StepYawFrom + 180.f;
	const float LookBack = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((After + 0.2f) / 1.0f, 0.f, 1.f));
	const float TurnIsland = FMath::SmoothStep(0.f, 1.f, FMath::Clamp((After - 2.6f) / 1.6f, 0.f, 1.f));
	if (After > -0.2f)
	{
		const float Y1 = StepYawFrom + FMath::FindDeltaAngleDegrees(StepYawFrom, BackYaw) * LookBack;
		Yaw = Y1 + FMath::FindDeltaAngleDegrees(Y1, StepYawTo) * TurnIsland;
		Pitch = FMath::Lerp(FMath::Lerp(-10.f, 2.f, LookBack), -2.f, TurnIsland);
	}
	else
	{
		Pitch = FMath::Lerp(-10.f, -16.f, SA);
	}
	C->SetControlRotation(FRotator(Pitch, Yaw, 0.f));

	if (After >= 4.6f) Finish(false);
}
