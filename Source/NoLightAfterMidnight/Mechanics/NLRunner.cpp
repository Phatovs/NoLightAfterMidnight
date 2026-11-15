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
#include "NLHideDoor.h"
#include "Components/PointLightComponent.h"

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

	FaceLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FaceLight"));
	FaceLight->SetupAttachment(GetCapsuleComponent());
	FaceLight->SetRelativeLocation(FVector(38.f, 0.f, 20.f));   // yüzün önü-altı
	FaceLight->SetIntensityUnits(ELightUnits::Candelas);
	FaceLight->SetIntensity(6.f);
	FaceLight->SetAttenuationRadius(140.f);
	FaceLight->SetLightColor(FLinearColor(0.75f, 0.82f, 1.f));
	FaceLight->SetCastShadows(false);
	FaceLight->SetVisibility(false);
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

	// Kapının dışında zorluyor (kapı aktörü yönetir)
	if (bDoorAssault) return;

	// Bekliyor: oyuncu tamirden (ya da başka bir işten) çıkınca koşmaya başla
	if (bPendingChase)
	{
		ANLPlayerCharacter* Waiting = ChaseTarget.Get();
		if (!Waiting || Waiting->GetMode() == ENLPlayerMode::Dead)
		{
			bPendingChase = false;
			ChaseTarget.Reset();
		}
		else if (Waiting->GetMode() != ENLPlayerMode::Repairing)
		{
			StartChase(Waiting, PendingSpawn);
		}
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
	if (ChaseDuration > 0.f && ChaseTimer <= 0.f)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RetreatSound, GetActorLocation());
		StopChase();
	}
}

void ANLRunner::PrepareChase(ANLPlayerCharacter* Target)
{
	if (bChasing || bPendingChase || !Target) return;

	ChaseTarget = Target;
	bPendingChase = true;
	PendingSpawn = FindSpawnPoint(Target);
	// Uzaktan gelen çığlık: geliyor. Henüz koşmaz, oyuncu tamirden çıkınca başlar.
	UGameplayStatics::PlaySoundAtLocation(this, ChaseStartScream, PendingSpawn.GetLocation());
	UE_LOG(LogNLRunner, Log, TEXT("Runner spawned far away at %s (%.0f cm), waiting for the player to leave the repair"),
		*PendingSpawn.GetLocation().ToCompactString(), FVector::Dist(PendingSpawn.GetLocation(), Target->GetActorLocation()));
}

FTransform ANLRunner::FindSpawnPoint(const ANLPlayerCharacter* Target) const
{
	// Oyuncunun ulaşılabilir NavMesh noktalarından: yeterince uzak ve tercihen görüş dışı olanı seç
	FTransform Best = HomeTransform;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav || !Target) return Best;

	const FVector From = Target->GetActorLocation();
	const FVector Eye = From + FVector(0.f, 0.f, 60.f);
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float BestScore = -1.f;
	for (int32 i = 0; i < 32; ++i)
	{
		FNavLocation P;
		if (!Nav->GetRandomReachablePointInRadius(From, SpawnMaxDistance, P)) continue;
		const float D = FVector::Dist2D(P.Location, From);
		if (D < SpawnMinDistance) continue;

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NLRunnerSpawn), false, Target);
		const bool bOutOfSight = GetWorld()->LineTraceSingleByChannel(Hit, Eye, P.Location + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params);
		const float Score = (bOutOfSight ? 10000.f : 0.f) + D;
		if (Score > BestScore)
		{
			BestScore = Score;
			const FVector Loc = P.Location + FVector(0.f, 0.f, HalfHeight + 2.f);
			Best = FTransform(FRotator(0.f, (From - Loc).Rotation().Yaw, 0.f), Loc);
		}
	}
	return Best;
}

void ANLRunner::StartChase(ANLPlayerCharacter* Target, const FTransform& From)
{
	if (bChasing || !Target) return;
	bPendingChase = false;

	ChaseTarget = Target;
	bChasing = true;
	ChaseTimer = ChaseDuration;
	RepathTimer = 0.f;

	SetActorTransform(From, false, nullptr, ETeleportType::TeleportPhysics);
	SetDormant(false);
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
	if (bChasing) UE_LOG(LogNLRunner, Log, TEXT("Runner chase ended"));
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
		FaceLight->SetVisibility(false);
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

void ANLRunner::BeginDoorAssault(ANLHideDoor* Door)
{
	if (!Door) return;
	UE_LOG(LogNLRunner, Log, TEXT("Runner reached the door, forcing it"));
	bChasing = false;
	bPendingChase = false;
	bDoorAssault = true;
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();

	// Kapının dışında, kapıya dönük dur (kapı kapalı: görünmez ama kapı aralandıkça seçilebilir)
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Loc = Door->GetOutsideLocation() + FVector(0.f, 0.f, HalfHeight + 2.f);
	const FRotator Face(0.f, Door->GetActorRotation().Yaw, 0.f);
	SetActorLocationAndRotation(Loc, Face, false, nullptr, ETeleportType::TeleportPhysics);
	SetDormant(false);
	GetCharacterMovement()->DisableMovement();
	ChaseAudio->Stop();
}

void ANLRunner::LeaveDoor()
{
	UE_LOG(LogNLRunner, Log, TEXT("Runner gave up on the door and left"));
	bDoorAssault = false;
	StopChase();
}

void ANLRunner::DoorKill(ANLPlayerCharacter* Target)
{
	bDoorAssault = false;
	ChaseTarget = Target;
	SetDormant(false);
	FaceLight->SetVisibility(true);
	KillTarget();
}
