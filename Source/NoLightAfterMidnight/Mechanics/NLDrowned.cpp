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
	Pose = EPose::None;

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
