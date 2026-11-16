#include "NLSaboteur.h"
#include "NLBodyRigComponent.h"
#include "NLPowerSubsystem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogNLSaboteur, Log, All);

ANLSaboteur::ANLSaboteur()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> MeshFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> DarkFinder(TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.MI_Manny_02_New"));

	// Görünmez sürücü: yürüme/koşu animasyonu
	USkeletalMeshComponent* Driver = GetMesh();
	Driver->SetSkeletalMeshAsset(MeshFinder.Get());
	if (AnimFinder.Succeeded())
	{
		Driver->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Driver->SetAnimInstanceClass(AnimFinder.Class);
	}
	Driver->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	Driver->SetHiddenInGame(true);
	Driver->SetCastShadow(false);
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Görünen gövde: pozlar (çömelme, kurcalama) kodla
	BodyRig = CreateDefaultSubobject<UNLBodyRigComponent>(TEXT("BodyRig"));
	BodyRig->SetupAttachment(GetCapsuleComponent());
	BodyRig->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
	BodyRig->bHideHead = false;
	BodyRig->SetCastShadow(true);
	if (UMaterialInterface* Dark = DarkFinder.Get())
	{
		BodyRig->SetMaterial(0, Dark);
		BodyRig->SetMaterial(1, Dark);
	}

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

	// Oyuncuyu itmesin / oyuncu ona takılmasın
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ANLSaboteur::BeginPlay()
{
	Super::BeginPlay();

	if (USkeletalMesh* Body = GetMesh()->GetSkeletalMeshAsset())
	{
		BodyRig->SetSkinnedAssetAndUpdate(Body);
	}
	BodyRig->SetDriver(GetMesh());
	FNLBodyPoseInput& In = BodyRig->GetPoseInput();
	In.bHoldFlashlight = false;
	In.IdleArmHold = 0.f;

	Vanish();
}

bool ANLSaboteur::RequestSabotage(UWorld* World, AActor* InTarget, TFunction<void()> InApply)
{
	if (!World || !InTarget) return false;

	ANLSaboteur* Free = nullptr;
	for (TActorIterator<ANLSaboteur> It(World); It; ++It)
	{
		if (!It->IsBusy()) { Free = *It; break; }
		return false;  // biri zaten iş başında: ikinci arıza aynı anda olmaz
	}
	if (!Free)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Free = World->SpawnActor<ANLSaboteur>(ANLSaboteur::StaticClass(), FTransform(FVector(0.f, 0.f, -20000.f)), P);
		if (Free && !Free->GetController()) Free->SpawnDefaultController();
	}
	if (!Free) return false;

	Free->Begin(InTarget, MoveTemp(InApply));
	return Free->IsBusy();
}

void ANLSaboteur::Begin(AActor* InTarget, TFunction<void()> InApply)
{
	Target = InTarget;
	ApplyFn = MoveTemp(InApply);
	bApplied = false;

	FVector Spawn, Approach;
	if (!FindApproachPoint(InTarget, Approach) || !FindHiddenNavPoint(Approach, SpawnMinDistance, SpawnMaxDistance, Spawn))
	{
		UE_LOG(LogNLSaboteur, Warning, TEXT("Saboteur: no spawn/approach point for %s, applying directly"), *GetNameSafe(InTarget));
		Apply();
		ApplyFn = nullptr;
		Target.Reset();
		return;
	}
	ApproachPoint = Approach;

	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocationAndRotation(Spawn + FVector(0.f, 0.f, Half + 2.f), FRotator(0.f, (Approach - Spawn).Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Crouch = 0.6f;

	EnterState(EState::Sneaking);
	MoveTo(ApproachPoint, SneakSpeed);
	UE_LOG(LogNLSaboteur, Log, TEXT("Saboteur: sneaking to %s from %.0f cm away"), *GetNameSafe(InTarget), FVector::Dist(Spawn, Approach));
}

void ANLSaboteur::EnterState(EState NewState)
{
	State = NewState;
	StateTime = 0.f;
}

void ANLSaboteur::Apply()
{
	if (bApplied) return;
	bApplied = true;
	if (ApplyFn) ApplyFn();
	UE_LOG(LogNLSaboteur, Log, TEXT("Saboteur: sabotaged %s"), *GetNameSafe(Target.Get()));
}

void ANLSaboteur::Vanish()
{
	EnterState(EState::Idle);
	Target.Reset();
	ApplyFn = nullptr;
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->DisableMovement();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	// Gizlemek yetmez (gölge önbelleği): haritanın altına taşı
	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, -20000.f), false, nullptr, ETeleportType::TeleportPhysics);
	FNLBodyPoseInput& In = BodyRig->GetPoseInput();
	In.CrouchAlpha = In.ReachAlphaL = In.ReachAlphaR = In.ExtraLean = 0.f;
}

void ANLSaboteur::MoveTo(const FVector& Point, float Speed)
{
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		const EPathFollowingRequestResult::Type R = AI->MoveToLocation(Point, 15.f, false, true, false);
		UE_LOG(LogNLSaboteur, Log, TEXT("Saboteur: move to %s -> %d"), *Point.ToCompactString(), int32(R));
	}
}

bool ANLSaboteur::FindHiddenNavPoint(const FVector& Around, float MinDist, float MaxDist, FVector& Out) const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav) return false;

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Eye = Player ? Player->GetActorLocation() + FVector(0.f, 0.f, 60.f) : FVector::ZeroVector;

	float BestScore = -1.f;
	for (int32 i = 0; i < 40; ++i)
	{
		FNavLocation P;
		if (!Nav->GetRandomReachablePointInRadius(Around, MaxDist, P)) continue;
		const float D = FVector::Dist2D(P.Location, Around);
		if (D < MinDist) continue;

		// Oyuncu görmüyorsa çok daha iyi; oyuncudan da uzak olsun
		bool bOutOfSight = true;
		float FromPlayer = 2000.f;
		if (Player)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(NLSaboteurSpot), false, Player);
			bOutOfSight = GetWorld()->LineTraceSingleByChannel(Hit, Eye, P.Location + FVector(0.f, 0.f, 90.f), ECC_Visibility, Params);
			FromPlayer = FVector::Dist2D(P.Location, Player->GetActorLocation());
		}
		const float Score = (bOutOfSight ? 100000.f : 0.f) + FMath::Min(FromPlayer, 1500.f) * 2.f + D * 0.2f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Out = P.Location;
		}
	}
	return BestScore >= 0.f;
}

bool ANLSaboteur::FindApproachPoint(const AActor* InTarget, FVector& Out) const
{
	// Hedefin çevresindeki 8 yönde, NavMesh üzerinde olan ve hedefe arada duvar olmadan bakan noktayı seç
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav || !InTarget) return false;

	const FBox Box = InTarget->GetComponentsBoundingBox();
	const FVector C = Box.GetCenter();
	const float Radius = FMath::Max(Box.GetExtent().X, Box.GetExtent().Y) + 55.f;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);

	bool bFound = false;
	float BestScore = -1.f;
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = 2.f * PI * i / 8.f;
		const FVector Probe = C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * Radius;
		FNavLocation P;
		if (!Nav->ProjectPointToNavigation(Probe, P, FVector(40.f, 40.f, 400.f))) continue;

		// Arada duvar var mı? (göğüs hizasından hedefin merkezine)
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NLSaboteurApproach), false, this);
		if (Player) Params.AddIgnoredActor(Player);
		const FVector Chest = P.Location + FVector(0.f, 0.f, 110.f);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Chest, C, ECC_Visibility, Params);
		if (bHit && Hit.GetActor() != InTarget) continue;

		// Hedefe yakın olanı tercih et
		const float Score = 1000.f - FVector::Dist2D(P.Location, C);
		if (Score > BestScore)
		{
			BestScore = Score;
			Out = P.Location;
			bFound = true;
		}
	}
	return bFound;
}

void ANLSaboteur::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == EState::Idle) return;

	StateTime += DeltaSeconds;

	// İş başındayken diğer arıza başlamaz (ve bittikten sonra arıza aralığı sayılır)
	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();

	AActor* T = Target.Get();
	const FVector Loc = GetActorLocation();
	float WantCrouch = 0.f, WantHands = 0.f, Lean = 0.f;

	if (State == EState::Sneaking)
	{
		// Çömelerek, öne eğilerek sinsice yürür
		WantCrouch = 1.f;
		Lean = 16.f;
		const bool bArrived = FVector::Dist2D(Loc, ApproachPoint) < 45.f;
		if (FMath::FloorToInt(StateTime / 2.f) != FMath::FloorToInt((StateTime - DeltaSeconds) / 2.f))
		{
			UE_LOG(LogNLSaboteur, Verbose, TEXT("Saboteur sneaking: at %s, %.0f cm to go, speed %.0f"), *Loc.ToCompactString(), FVector::Dist2D(Loc, ApproachPoint), GetVelocity().Size2D());
		}
		const bool bStuck = StateTime > GiveUpTime;
		if (bArrived || bStuck || !T)
		{
			if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
			if (T)
			{
				// Kurcalanacak nokta: hedefin göğse en yakın yüzü
				const FBox Box = T->GetComponentsBoundingBox();
				const FVector Chest = Loc + FVector(0.f, 0.f, 35.f);
				HandPoint = Box.GetClosestPointTo(Chest);
				HandPoint += (Chest - HandPoint).GetSafeNormal() * 4.f;
			}
			EnterState(EState::Tampering);
		}
	}
	else if (State == EState::Tampering)
	{
		// Hedefe dön, elleriyle kurcala; alçak hedefte eğilir
		if (T)
		{
			const FRotator Face(0.f, (HandPoint - Loc).Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Face, DeltaSeconds, 8.f));

			// Hedefe uzanabileceği kadar yaklaş (bir adım): eller havada kalmasın
			FVector ToHand = HandPoint - Loc;
			ToHand.Z = 0.f;
			const float Gap = ToHand.Size() - TamperReach;
			if (Gap > 2.f)
			{
				const FVector Step = ToHand.GetSafeNormal() * FMath::Min(Gap, 120.f * DeltaSeconds);
				SetActorLocation(Loc + Step, true);
			}
		}
		const float FloorZ = Loc.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const bool bLowTarget = HandPoint.Z < FloorZ + 105.f;
		WantCrouch = bLowTarget ? 1.f : 0.2f;
		WantHands = 1.f;
		Lean = bLowTarget ? 22.f : 6.f;

		if (StateTime >= TamperDuration * 0.75f) Apply();
		if (StateTime >= TamperDuration)
		{
			// Doğrul ve koşarak görüş dışına kaç
			FVector Flee;
			if (!FindHiddenNavPoint(Loc, 1000.f, 2600.f, Flee)) Flee = Loc - GetActorForwardVector() * 1000.f;
			FleePoint = Flee;
			EnterState(EState::Fleeing);
			MoveTo(FleePoint, FleeSpeed);
		}
	}
	else if (State == EState::Fleeing)
	{
		WantCrouch = 0.f;
		Lean = 10.f;
		if (FVector::Dist2D(Loc, FleePoint) < 80.f || StateTime > 9.f)
		{
			UE_LOG(LogNLSaboteur, Log, TEXT("Saboteur: escaped and vanished"));
			Vanish();
			return;
		}
	}

	Crouch = FMath::FInterpTo(Crouch, WantCrouch, DeltaSeconds, 5.f);
	Hands = FMath::FInterpTo(Hands, WantHands, DeltaSeconds, 7.f);

	FNLBodyPoseInput& In = BodyRig->GetPoseInput();
	In.View = GetActorTransform();
	In.bHoldFlashlight = false;
	In.CrouchAlpha = Crouch;
	In.CrouchDepth = SneakCrouchDepth;
	In.ExtraLean = Lean * FMath::Max(Crouch, 0.4f);
	// Sinsice yürürken kollar gövdeye yakın, kurcalarken eller hedefte titrer
	In.IdleArmHold = State == EState::Sneaking ? 0.6f : 0.f;
	const FVector Right = GetActorRightVector();
	const FVector Fiddle = FVector(0.f, 0.f, FMath::Sin(StateTime * 17.f) * 3.f) + Right * FMath::Cos(StateTime * 13.f) * 2.f;
	In.ReachAlphaL = In.ReachAlphaR = Hands;
	In.ReachTargetL = HandPoint - Right * 11.f + Fiddle;
	In.ReachTargetR = HandPoint + Right * 11.f - Fiddle;
}
