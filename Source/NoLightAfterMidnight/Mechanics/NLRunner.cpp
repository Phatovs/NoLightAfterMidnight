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

ANLRunner::ANLRunner()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(ChaseStartScream, "runner_scream");
	NL_DEFAULT_SOUND(ChaseLoopSound, "runner_chase_loop");
	NL_DEFAULT_SOUND(KillSound, "kill");

	// UE mankeni (Manny/Quinn) kapsülün altına ve ileri bakacak şekilde hizala
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

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

	SetDormant(true);
}

void ANLRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
			AI->MoveToActor(Target, 10.f, false);
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

	if (SpawnMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Play(SpawnMontage);
	}
}

void ANLRunner::StopChase()
{
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
	ANLPlayerCharacter* Target = ChaseTarget.Get();
	bChasing = false;

	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->DisableMovement();
	ChaseAudio->Stop();

	// Oyuncuya dön
	if (Target)
	{
		const FVector ToPlayer = Target->GetActorLocation() - GetActorLocation();
		SetActorRotation(FRotator(0.f, ToPlayer.Rotation().Yaw, 0.f));
		Target->Die();
		Target->LookAtKiller(this);
	}

	UGameplayStatics::PlaySound2D(this, KillSound);

	float KillTime = FallbackKillDuration;
	if (KillMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			const float Len = Anim->Montage_Play(KillMontage);
			if (Len > 0.f) KillTime = Len;
		}
	}

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		GM->TriggerGameOver(ENLGameOverReason::Runner, KillTime);
	}
}
