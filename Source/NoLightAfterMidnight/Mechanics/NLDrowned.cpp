#include "NLDrowned.h"
#include "NLDefaults.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "NLBodyRigComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

ANLDrowned::ANLDrowned()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(AppearSound, "drowned_appear");
	NL_DEFAULT_SOUND(BreathLoopSound, "drowned_breath_loop");
	NL_DEFAULT_SOUND(ScreechSound, "drowned_screech");
	NL_DEFAULT_SOUND(JumpscareSound, "jumpscare");

	static ConstructorHelpers::FObjectFinderOptional<UAnimSequenceBase> RunFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
	FleeRunAnimation = RunFinder.Get();

	// UE mankeni (Manny/Quinn) kapsülün altına ve ileri bakacak şekilde hizala
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;

	// GetMesh() görünmez sürücü (bekleme animasyonu); görünen gövde BodyRig, pozlar kodla verilir
	GetMesh()->SetHiddenInGame(true);
	GetMesh()->SetCastShadow(false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	BodyRig = CreateDefaultSubobject<UNLBodyRigComponent>(TEXT("BodyRig"));
	BodyRig->SetupAttachment(GetCapsuleComponent());
	BodyRig->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	BodyRig->bHideHead = false;
	BodyRig->SetCastShadow(true);

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(GetMesh());
	BreathAudio->bAutoActivate = false;
}

void ANLDrowned::BeginPlay()
{
	Super::BeginPlay();

	// Drowned yürümez, kameralar arasında "belirir". Fizik/çarpışma gerekmez.
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	if (BreathLoopSound) BreathAudio->SetSound(BreathLoopSound);

	if (USkeletalMesh* Body = GetMesh()->GetSkeletalMeshAsset())
	{
		BodyRig->SetSkinnedAssetAndUpdate(Body);
	}
	BodyRig->SetRelativeLocationAndRotation(GetMesh()->GetRelativeLocation(), GetMesh()->GetRelativeRotation());
	BodyRig->SetDriver(GetMesh());
	BodyRig->GetPoseInput().bHoldFlashlight = false;

	Vanish();
}

void ANLDrowned::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bShown && bRunning) UpdateRun(DeltaSeconds);
	if (bShown) UpdatePose(DeltaSeconds);
}

void ANLDrowned::StartPose(EPose NewPose, float Duration)
{
	Pose = NewPose;
	PoseTimer = 0.f;
	PoseDuration = FMath::Max(Duration, 0.1f);
	FleeStart = GetActorLocation();
	FleeStartRot = GetActorRotation();
}

void ANLDrowned::UpdatePose(float DeltaSeconds)
{
	FNLBodyPoseInput& In = BodyRig->GetPoseInput();
	In.bHoldFlashlight = false;
	In.View = GetActorTransform();
	In.ReachAlphaL = In.ReachAlphaR = 0.f;
	In.ExtraLean = 0.f;
	In.ElbowsOut = 0.f;
	if (Pose == EPose::None) return;

	PoseTimer += DeltaSeconds;
	const float T = FMath::Clamp(PoseTimer / PoseDuration, 0.f, 1.f);
	const FVector Fwd = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const FVector Up = FVector::UpVector;
	const FVector Head = BodyRig->GetSocketLocation(TEXT("head"));

	if (Pose == EPose::CoverFace)
	{
		// Işık yiyince: eller hızla yüze siper olur, gövde geriye kaçar, titrer
		const float Raise = FMath::SmoothStep(0.f, 0.18f, T);
		const float Shake = FMath::Sin(PoseTimer * 55.f) * 2.f * Raise;
		In.ReachAlphaL = In.ReachAlphaR = Raise;
		In.ReachTargetL = Head + Fwd * 14.f - Right * 5.f + Up * (3.f + Shake);
		In.ReachTargetR = Head + Fwd * 15.f + Right * 4.f + Up * (7.f - Shake);
		In.ExtraLean = -FlinchLeanBack * Raise;
	}
	else if (Pose == EPose::Flee)
	{
		// Dönüp kaçar: önce arkasını döner, sonra uzaklaşır; eller yüzden düşer
		const float Turn = FMath::SmoothStep(0.f, 0.3f, T);
		const float Move = FMath::SmoothStep(0.18f, 1.f, T);
		SetActorRotation(FRotator(0.f, FleeStartRot.Yaw + 180.f * Turn, 0.f));
		SetActorLocation(FleeStart - FleeStartRot.Vector() * (FleeMaxDistance * Move));
		const float Hands = 1.f - FMath::SmoothStep(0.f, 0.35f, T);
		In.ReachAlphaL = In.ReachAlphaR = Hands;
		In.ReachTargetL = Head + Fwd * 14.f - Right * 5.f + Up * 3.f;
		In.ReachTargetR = Head + Fwd * 15.f + Right * 4.f + Up * 7.f;
		In.ExtraLean = -FlinchLeanBack * Hands + 8.f * Move;  // kaçarken öne atılır
	}
	else if (Pose == EPose::Jumpscare)
	{
		// Ortak yakalama hareketi (Runner ile aynı)
		Grab.Update(this, PoseTimer, T, In);
	}
}

void ANLDrowned::AppearAt(const FTransform& SpawnTransform)
{
	// Spawn noktasından aşağı doğru zemini bul, ayaklar zemine bassın
	FVector Loc = SpawnTransform.GetLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NLDrownedGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 50.f), Loc - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		Loc = Hit.ImpactPoint;
	}
	Loc.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocationAndRotation(Loc, SpawnTransform.GetRotation().Rotator(), false, nullptr, ETeleportType::TeleportPhysics);

	SetActorHiddenInGame(false);
	bShown = true;
	bRunning = false;
	bArrived = true;
	Pose = EPose::None;
	StopRunAnimation();
	RouteStart = Loc;

	UGameplayStatics::PlaySoundAtLocation(this, AppearSound, GetActorLocation());
	BreathAudio->Play();
	PlayMontageSafe(AppearMontage, 0.f);
}

float ANLDrowned::PlayCoverFace()
{
	// Kamerada izlenirken duyulsun diye 2D
	UGameplayStatics::PlaySound2D(this, ScreechSound);
	const float Len = PlayMontageSafe(CoverFaceMontage, -1.f);
	if (Len > 0.f) return Len;
	StartPose(EPose::CoverFace, FallbackCoverFaceDuration);
	return FallbackCoverFaceDuration;
}

float ANLDrowned::PlayFlee()
{
	const float Len = PlayMontageSafe(FleeMontage, -1.f);
	if (Len > 0.f) return Len;
	StartPose(EPose::Flee, FallbackFleeDuration);

	// Arkasında duvar varsa ona çarpmadan dur
	FleeMaxDistance = FleeDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NLDrownedFlee), false, this);
	const FVector From = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, From - GetActorForwardVector() * (FleeDistance + 40.f), ECC_Visibility, Params))
	{
		FleeMaxDistance = FMath::Max(0.f, Hit.Distance - 40.f);
	}

	// Koşu: sürücü mesh bu süre boyunca koşu animasyonunu oynatır
	if (FleeRunAnimation)
	{
		GetMesh()->PlayAnimation(FleeRunAnimation, true);
		GetMesh()->SetPlayRate(FleeRunPlayRate);
		bRunAnimPlaying = true;
	}
	return FallbackFleeDuration;
}

void ANLDrowned::Vanish()
{
	bShown = false;
	bRunning = false;
	bArrived = false;
	Pose = EPose::None;
	Grab.Stop();
	StopRunAnimation();
	SetActorHiddenInGame(true);
	BreathAudio->Stop();
	// Gizlemek yetmez: gölge önbelleği eski gölgeyi bırakabilir. Haritanın altına taşı ki gölge kesin silinsin.
	SetActorLocation(GetActorLocation() - FVector(0.f, 0.f, 20000.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->StopAllMontages(0.f);
	}
}

float ANLDrowned::PerformJumpscare(APlayerController* Victim)
{
	SetActorHiddenInGame(false);
	bShown = true;
	bRunning = false;
	StopRunAnimation();
	BreathAudio->Stop();
	UGameplayStatics::PlaySound2D(this, JumpscareSound);

	// Kameranın önüne ışınlan (yüz ekranın ortasında)
	Grab.Distance = JumpscareDistance;
	Grab.HeadBone = HeadBoneName;
	Grab.FaceCenterOffset = FaceCenterOffset;
	Grab.FallbackHeadHeight = FallbackHeadHeight;
	Grab.Lunge = JumpscareLunge;
	Grab.CameraShake = JumpscareCameraShake;
	Grab.Begin(this, GetMesh(), Victim);

	const float Len = PlayMontageSafe(JumpscareMontage, -1.f);
	if (Len > 0.f)
	{
		Grab.Stop();
		return Len;
	}
	// Montaj yoksa kodla: kollarıyla yakalar
	StartPose(EPose::Jumpscare, FallbackJumpscareDuration);
	return FallbackJumpscareDuration;
}

float ANLDrowned::PlayMontageSafe(UAnimMontage* Montage, float Fallback)
{
	if (Montage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			const float Len = Anim->Montage_Play(Montage);
			if (Len > 0.f) return Len;
		}
	}
	return Fallback;
}

void ANLDrowned::StopRunAnimation()
{
	if (!bRunAnimPlaying) return;
	bRunAnimPlaying = false;
	// Normal bekleme animasyonuna (AnimBP) geri dön
	GetMesh()->SetPlayRate(1.f);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
}

// ------------------------------------------------------------------ Hat üzerinde koşma

bool ANLDrowned::FindRoute(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, To);
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2) return false;
	OutPoints = Path->PathPoints;
	return true;
}

void ANLDrowned::ApproachTo(const FTransform& SpawnTransform)
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const FVector Spot = SpawnTransform.GetLocation();
	// Spawn noktası kameraya bakar: hattın başı noktanın gerisinde (kameradan uzakta), Drowned kameraya doğru koşar
	const FVector Back = -SpawnTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	FNavLocation SpotNav;
	TArray<FVector> Points;
	if (Nav && Nav->ProjectPointToNavigation(Spot, SpotNav, FVector(150.f, 150.f, 400.f)))
	{
		for (float Dist = ApproachDistance; Dist >= ApproachDistance * 0.35f && Points.Num() == 0; Dist -= ApproachDistance * 0.2f)
		{
			FNavLocation StartNav;
			if (!Nav->ProjectPointToNavigation(Spot + Back * Dist, StartNav, FVector(500.f, 500.f, 800.f))) continue;
			FindRoute(StartNav.Location, SpotNav.Location, Points);
		}
	}
	if (Points.Num() < 2)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDrowned: no route to %s, appearing in place"), *Spot.ToCompactString());
		AppearAt(SpawnTransform);
		return;
	}

	ArriveRotation = SpawnTransform.GetRotation().Rotator();
	RouteStart = Points[0];
	SetActorLocationAndRotation(Points[0] + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
		FRotator(0.f, (Points[1] - Points[0]).Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	bShown = true;
	bArrived = false;
	Pose = EPose::None;
	BreathAudio->Play();
	StartRun(Points, false);
	UE_LOG(LogTemp, Log, TEXT("NLDrowned: running to the camera (%d points, %.0f cm)"), Points.Num(), FVector::Dist(Points[0], Points.Last()));
}

void ANLDrowned::RunOffAndVanish()
{
	if (!bShown) return;
	TArray<FVector> Points;
	FNavLocation Here;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (Nav && Nav->ProjectPointToNavigation(GetActorLocation(), Here, FVector(150.f, 150.f, 400.f)) && FVector::Dist2D(Here.Location, RouteStart) > 200.f)
	{
		FindRoute(Here.Location, RouteStart, Points);
	}
	if (Points.Num() < 2)
	{
		Vanish();
		return;
	}
	Pose = EPose::None;
	StartRun(Points, true);
}

void ANLDrowned::FaceTowards(const FVector& Location)
{
	const FVector To = (Location - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero()) SetActorRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
	bRunning = false;
	StopRunAnimation();
}

void ANLDrowned::StartRun(const TArray<FVector>& Points, bool bOff)
{
	RunPath = Points;
	RunIndex = 1;
	RunTimer = 0.f;
	bRunOff = bOff;
	bRunning = true;
	if (FleeRunAnimation)
	{
		GetMesh()->PlayAnimation(FleeRunAnimation, true);
		GetMesh()->SetPlayRate(FleeRunPlayRate * RunSpeed / 380.f);
		bRunAnimPlaying = true;
	}
}

void ANLDrowned::UpdateRun(float DeltaSeconds)
{
	RunTimer += DeltaSeconds;
	if (bRunOff && RunTimer > RunOffMaxTime)
	{
		Vanish();
		return;
	}

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Loc = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	float Step = RunSpeed * DeltaSeconds;
	while (Step > 0.f && RunIndex < RunPath.Num())
	{
		const FVector Target = RunPath[RunIndex];
		const float D = FVector::Dist(Loc, Target);
		if (D <= Step) { Loc = Target; Step -= D; ++RunIndex; }
		else { Loc += (Target - Loc) / D * Step; Step = 0.f; }
	}
	// Koştuğu yöne döner (yumuşak)
	const FVector Dir = RunIndex < RunPath.Num() ? (RunPath[RunIndex] - Loc).GetSafeNormal2D() : FVector::ZeroVector;
	FRotator Rot = GetActorRotation();
	if (!Dir.IsNearlyZero()) Rot = FMath::RInterpTo(Rot, FRotator(0.f, Dir.Rotation().Yaw, 0.f), DeltaSeconds, 8.f);
	SetActorLocationAndRotation(Loc + FVector(0.f, 0.f, HalfHeight), Rot, false, nullptr, ETeleportType::TeleportPhysics);

	if (RunIndex >= RunPath.Num())
	{
		bRunning = false;
		StopRunAnimation();
		if (bRunOff)
		{
			Vanish();
			return;
		}
		// Kameranın önüne vardı: kameraya döner ve bekler
		SetActorRotation(ArriveRotation);
		bArrived = true;
		UGameplayStatics::PlaySoundAtLocation(this, AppearSound, GetActorLocation());
		PlayMontageSafe(AppearMontage, 0.f);
	}
}
