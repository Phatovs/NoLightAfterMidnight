#include "NLRunner.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "AIController.h"
#include "Components/SphereComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NLBodyRigComponent.h"
#include "Engine/SkeletalMesh.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogNLRunner, Log, All);

ANLRunner::ANLRunner()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(ChaseStartScream, "runner_scream");
	NL_DEFAULT_SOUND(ChaseLoopSound, "runner_chase_loop");
	NL_DEFAULT_SOUND(KillSound, "kill");
	NL_DEFAULT_SOUND(RetreatSound, "runner_retreat");

	// UE mankeni (Manny/Quinn) kapsülün altına ve ileri bakacak şekilde hizala
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

	// GetMesh() görünmez sürücü (koşu animasyonu); görünen gövde BodyRig
	GetMesh()->SetHiddenInGame(true);
	GetMesh()->SetCastShadow(false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	BodyRig = CreateDefaultSubobject<UNLBodyRigComponent>(TEXT("BodyRig"));
	BodyRig->SetupAttachment(GetCapsuleComponent());
	BodyRig->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	BodyRig->bHideHead = false;
	BodyRig->SetCastShadow(true);

	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);

	KillSphere = CreateDefaultSubobject<USphereComponent>(TEXT("KillSphere"));
	KillSphere->SetupAttachment(GetCapsuleComponent());
	KillSphere->SetSphereRadius(80.f);
	KillSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	ChaseAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ChaseAudio"));
	ChaseAudio->SetupAttachment(GetCapsuleComponent());
	ChaseAudio->bAutoActivate = false;
}

void ANLRunner::BeginPlay()
{
	Super::BeginPlay();

	HomeTransform = GetActorTransform();
	KillSphere->SetSphereRadius(KillRadius);
	KillSphere->OnComponentBeginOverlap.AddDynamic(this, &ANLRunner::OnKillSphereOverlap);
	if (ChaseLoopSound) ChaseAudio->SetSound(ChaseLoopSound);

	if (USkeletalMesh* Body = GetMesh()->GetSkeletalMeshAsset())
	{
		BodyRig->SetSkinnedAssetAndUpdate(Body);
	}
	BodyRig->SetRelativeLocationAndRotation(GetMesh()->GetRelativeLocation(), GetMesh()->GetRelativeRotation());
	BodyRig->SetDriver(GetMesh());
	FNLBodyPoseInput& In = BodyRig->GetPoseInput();
	In.bHoldFlashlight = false;
	In.IdleArmHold = 0.f;   // koşarken kollar serbestçe sallansın

	SetDormant(true);
}

void ANLRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Yakalama jumpscare (Boğulmuş ile aynı hareket)
	if (Grab.IsActive())
	{
		GrabTimer += DeltaSeconds;
		FNLBodyPoseInput& In = BodyRig->GetPoseInput();
		In.View = GetActorTransform();
		Grab.Update(this, GrabTimer, FMath::Clamp(GrabTimer / FMath::Max(GrabDuration, 0.1f), 0.f, 1.f), In);
		return;
	}

	if (!bChasing) return;

	ANLPlayerCharacter* Target = ChaseTarget.Get();
	if (!Target || Target->GetMode() == ENLPlayerMode::Dead)
	{
		StopChase();
		return;
	}

	RepathTimer -= DeltaSeconds;
	if (RepathTimer <= 0.f)
	{
		RepathTimer = RepathInterval;
		if (AAIController* AI = Cast<AAIController>(GetController()))
		{
			const EPathFollowingRequestResult::Type Result = AI->MoveToActor(Target, 10.f, false);
			UE_LOG(LogNLRunner, Verbose, TEXT("Runner at %s, dist=%.0f, speed=%.0f, move=%d"),
				*GetActorLocation().ToCompactString(), FVector::Dist(GetActorLocation(), Target->GetActorLocation()),
				GetVelocity().Size(), int32(Result));
		}
	}

	// Mesafe kontrolü (overlap kaçarsa yedek)
	if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) < FMath::Square(KillRadius))
	{
		KillTarget();
		return;
	}

	ChaseTimer -= DeltaSeconds;
	if (ChaseTimer <= 0.f)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RetreatSound, GetActorLocation());
		StopChase();
	}
}

void ANLRunner::StartChase(ANLPlayerCharacter* Target)
{
	if (bChasing || !Target) return;

	ChaseTarget = Target;
	bChasing = true;
	ChaseTimer = ChaseDuration;
	RepathTimer = 0.f;

	SetActorTransform(HomeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SetDormant(false);

	UGameplayStatics::PlaySound2D(this, ChaseStartScream);
	ChaseAudio->Play();

	// Teşhis: NavMesh var mı, oyuncuya yol bulunuyor mu?
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = Nav ? Nav->GetDefaultNavDataInstance() : nullptr;
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToActorSynchronously(this, GetActorLocation(), Target);
	UE_LOG(LogNLRunner, Log, TEXT("Chase started: controller=%s navdata=%s path=%s points=%d"),
		*GetNameSafe(GetController()), *GetNameSafe(NavData),
		Path && Path->IsValid() ? (Path->IsPartial() ? TEXT("partial") : TEXT("ok")) : TEXT("NONE"),
		Path ? Path->PathPoints.Num() : 0);
	if (!NavData)
	{
		UE_LOG(LogNLRunner, Warning, TEXT("Runner: no NavMesh in the level! Add a NavMeshBoundsVolume covering the area."));
	}

	if (SpawnMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Play(SpawnMontage);
	}
}

void ANLRunner::StopChase()
{
	if (bChasing) UE_LOG(LogNLRunner, Log, TEXT("Runner chase ended (time out or target lost)"));
	bChasing = false;
	ChaseTarget.Reset();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	SetDormant(true);
}

void ANLRunner::SetDormant(bool bDormant)
{
	SetActorHiddenInGame(bDormant);
	SetActorEnableCollision(!bDormant);
	if (bDormant)
	{
		// Gizlemek yetmez: gölge önbelleği eski gölgeyi bırakabilir. Haritanın altına taşı (StartChase evine geri koyar).
		Grab.Stop();
		FNLBodyPoseInput& In = BodyRig->GetPoseInput();
		In.ReachAlphaL = In.ReachAlphaR = In.ElbowsOut = In.ExtraLean = 0.f;
		SetActorLocation(GetActorLocation() - FVector(0.f, 0.f, 20000.f), false, nullptr, ETeleportType::TeleportPhysics);
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = ChaseSpeed;
	if (bDormant)
	{
		Move->DisableMovement();     // gizliyken düşmesin
		ChaseAudio->Stop();
	}
	else
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

void ANLRunner::OnKillSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bChasing && OtherActor && OtherActor == ChaseTarget.Get())
	{
		KillTarget();
	}
}

void ANLRunner::KillTarget()
{
	UE_LOG(LogNLRunner, Log, TEXT("Runner caught the player"));
	ANLPlayerCharacter* Target = ChaseTarget.Get();
	bChasing = false;

	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->DisableMovement();
	ChaseAudio->Stop();

	if (Target) Target->Die();
	UGameplayStatics::PlaySound2D(this, KillSound);

	float KillTime = FallbackKillDuration;
	bool bMontage = false;
	if (KillMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			const float Len = Anim->Montage_Play(KillMontage);
			if (Len > 0.f) { KillTime = Len; bMontage = true; }
		}
	}

	if (bMontage)
	{
		// Montaj varsa eski davranış: oyuncuya dön, oyuncu ona baksın
		if (Target)
		{
			const FVector ToPlayer = Target->GetActorLocation() - GetActorLocation();
			SetActorRotation(FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f));
			Target->LookAtKiller(this);
		}
	}
	else
	{
		// Boğulmuş ile aynı jumpscare: kameranın önüne atılır, kollarıyla yakalar
		SetActorEnableCollision(false);
		Grab.Distance = JumpscareDistance;
		Grab.Lunge = JumpscareLunge;
		Grab.CameraShake = JumpscareCameraShake;
		Grab.Begin(this, GetMesh(), Target ? Cast<APlayerController>(Target->GetController()) : nullptr);
		GrabTimer = 0.f;
		GrabDuration = FallbackKillDuration;
	}

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		GM->TriggerGameOver(ENLGameOverReason::Runner, KillTime);
	}
}
