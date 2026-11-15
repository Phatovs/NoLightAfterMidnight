#include "NLPlayerCharacter.h"
#include "NLDefaults.h"
#include "NLFlashlightComponent.h"
#include "NLInteractable.h"
#include "NLSecurityDesk.h"
#include "NLGenerator.h"
#include "NLHUDWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "NLBodyRigComponent.h"
#include "NLFuelCan.h"
#include "NLFuseBox.h"
#include "NLFuseBoxWidget.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Engine/GameViewportClient.h"
#include "NLFlashlightCharger.h"
#include "NLDrowned.h"
#include "NLSecurityCamera.h"
#include "NLRunner.h"
#include "NLSaboteur.h"
#include "NLHideDoor.h"
#include "NLGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"

ANLPlayerCharacter::ANLPlayerCharacter()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(FootstepSound, "footstep");
	NL_DEFAULT_SOUND(StandUpSound, "chair_stand");
	NL_DEFAULT_SOUND(DeathSound, "player_death");
	NL_DEFAULT_SOUND(ShockSound, "fuse_trip");
	NL_DEFAULT_SOUND(PantingLoop, "breath_panic_loop");
	NL_DEFAULT_SOUND(ExhaustedBreathLoop, "breath_exhausted_loop");

	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// --- Gövde: görünmez sürücü (yürüme animasyonu) + görünen, kodla pozlanan gövde ---
	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> BodyMeshFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> BodyAnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	USkeletalMeshComponent* Driver = GetMesh();
	Driver->SetSkeletalMeshAsset(BodyMeshFinder.Get());
	if (BodyAnimFinder.Succeeded())
	{
		Driver->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Driver->SetAnimInstanceClass(BodyAnimFinder.Class);
	}
	Driver->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	Driver->SetHiddenInGame(true);
	Driver->SetCastShadow(false);
	Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	BodyRig = CreateDefaultSubobject<UNLBodyRigComponent>(TEXT("BodyRig"));
	BodyRig->SetupAttachment(GetCapsuleComponent());
	BodyRig->SetRelativeLocationAndRotation(FVector(-BodyBackOffset, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));

	// Fener elde: pivot her karede sağ elin konumuna taşınır, ışık feneri ucundan çıkar
	FlashlightPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FlashlightPivot"));
	FlashlightPivot->SetupAttachment(GetCapsuleComponent());

	FlashlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightMesh"));
	FlashlightMesh->SetupAttachment(FlashlightPivot);
	FlashlightMesh->SetStaticMesh(CylinderFinder.Get());
	FlashlightMesh->SetRelativeLocationAndRotation(FVector(8.f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
	FlashlightMesh->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.22f));
	FlashlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashlightMesh->SetCastShadow(false);

	Flashlight = CreateDefaultSubobject<UNLFlashlightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FlashlightPivot);
	Flashlight->SetRelativeLocation(FVector(20.f, 0.f, 0.f));

	// Taşınan benzin bidonu (sol elde)
	CarryCanPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CarryCanPivot"));
	CarryCanPivot->SetupAttachment(GetCapsuleComponent());

	CarryCanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarryCanMesh"));
	CarryCanMesh->SetupAttachment(CarryCanPivot);
	CarryCanMesh->SetStaticMesh(CylinderFinder.Get());
	CarryCanMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	CarryCanMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.36f));
	CarryCanMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarryCanMesh->SetCastShadow(false);
	CarryCanPivot->SetVisibility(false, true);

	// Koştuktan sonra nefes nefese kalma (2D, sesi stamina azaldıkça artar)
	PantingAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("PantingAudio"));
	PantingAudio->SetupAttachment(GetCapsuleComponent());
	PantingAudio->bAutoActivate = false;
	PantingAudio->bAllowSpatialization = false;

	ExhaustedBreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ExhaustedBreathAudio"));
	ExhaustedBreathAudio->SetupAttachment(GetCapsuleComponent());
	ExhaustedBreathAudio->bAutoActivate = false;
	ExhaustedBreathAudio->bAllowSpatialization = false;

	bUseControllerRotationYaw = true;

	// Varsayılan: arayüzü tamamen C++'ta kuran HUD. İsterseniz kendi WBP'nizi atayabilirsiniz.
	HUDWidgetClass = UNLHUDWidget::StaticClass();
}

void ANLPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	if (USkeletalMesh* BodyAsset = GetMesh()->GetSkeletalMeshAsset())
	{
		BodyRig->SetSkinnedAssetAndUpdate(BodyAsset);
	}
	BodyRig->SetRelativeLocationAndRotation(FVector(-BodyBackOffset, 0.f, GetMesh()->GetRelativeLocation().Z), GetMesh()->GetRelativeRotation());
	BodyRig->SetDriver(GetMesh());
	BodyRig->SetHeldFlashlight(FlashlightPivot);
	BodyRig->SetHeldCan(CarryCanPivot);
	if (PantingLoop) PantingAudio->SetSound(PantingLoop);
	if (ExhaustedBreathLoop) ExhaustedBreathAudio->SetSound(ExhaustedBreathLoop);
}

void ANLPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	CreateHUD();
}

void ANLPlayerCharacter::CreateHUD()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || !HUDWidgetClass || HUDWidget) return;

	HUDWidget = CreateWidget<UNLHUDWidget>(PC, HUDWidgetClass);
	if (HUDWidget) HUDWidget->AddToViewport();
}

// ------------------------------------------------------------------ Input

UInputAction* ANLPlayerCharacter::MakeAction(const TCHAR* Name, bool bAxis2D)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	return Action;
}

void ANLPlayerCharacter::BuildInput()
{
	if (MappingContext) return;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_NL_Runtime"));

	MoveAction       = MakeAction(TEXT("IA_Move"), true);
	LookAction       = MakeAction(TEXT("IA_Look"), true);
	InteractAction   = MakeAction(TEXT("IA_Interact"), false);
	SpaceAction      = MakeAction(TEXT("IA_Space"), false);
	FlashlightAction = MakeAction(TEXT("IA_Flashlight"), false);
	CameraNextAction = MakeAction(TEXT("IA_CameraNext"), false);
	CameraPrevAction = MakeAction(TEXT("IA_CameraPrev"), false);

	// WASD -> 2D eksen: W=+Y, S=-Y, D=+X, A=-X
	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));

		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		FEnhancedActionKeyMapping& A = MappingContext->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		MappingContext->MapKey(MoveAction, EKeys::D);
	}

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(SpaceAction, EKeys::SpaceBar);
	MappingContext->MapKey(FlashlightAction, EKeys::F);
	MappingContext->MapKey(CameraNextAction, EKeys::Right);
	MappingContext->MapKey(CameraNextAction, EKeys::D);
	MappingContext->MapKey(CameraPrevAction, EKeys::Left);
	MappingContext->MapKey(CameraPrevAction, EKeys::A);

#if !UE_BUILD_SHIPPING
	DebugEndDayAction = MakeAction(TEXT("IA_DebugEndDay"), false);
	MappingContext->MapKey(DebugEndDayAction, EKeys::F9);
#endif

	const FKey NumberKeys[8] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
	                             EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
	for (int32 i = 0; i < 8; ++i)
	{
		UInputAction* Sel = MakeAction(*FString::Printf(TEXT("IA_Cam%d"), i + 1), false);
		CameraSelectActions.Add(Sel);
		MappingContext->MapKey(Sel, NumberKeys[i]);
	}
}

void ANLPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(MappingContext, 0);
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("NLPlayerCharacter: Enhanced Input aktif değil! Project Settings > Input > Default Input Component Class = EnhancedInputComponent olmalı."));
		return;
	}

	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANLPlayerCharacter::OnMove);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANLPlayerCharacter::OnLook);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnInteract);
	EIC->BindAction(SpaceAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnSpace);
	EIC->BindAction(FlashlightAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnFlashlight);
	EIC->BindAction(CameraNextAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnCameraNext);
	EIC->BindAction(CameraPrevAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnCameraPrev);
	for (UInputAction* Sel : CameraSelectActions)
	{
		EIC->BindAction(Sel, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnCameraSelect);
	}
	if (DebugEndDayAction)
	{
		EIC->BindAction(DebugEndDayAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnDebugEndDay);
	}
}

void ANLPlayerCharacter::OnDebugEndDay()
{
#if !UE_BUILD_SHIPPING
	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->NLDebugEndDayIn(5.f);
#endif
}

void ANLPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	if (Mode != ENLPlayerMode::Walking || !Controller || IsBusy()) return;

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.X);
}

void ANLPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	if (Mode == ENLPlayerMode::Seated || Mode == ENLPlayerMode::Dead || Mode == ENLPlayerMode::UsingFuseBox || Mode == ENLPlayerMode::HoldingDoor || IsBusy()) return;

	const FVector2D Axis = Value.Get<FVector2D>() * MouseSensitivity;
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ANLPlayerCharacter::OnInteract()
{
	switch (Mode)
	{
	case ENLPlayerMode::Seated:
		StandUp();
		return;

	case ENLPlayerMode::Repairing:
		if (ANLGenerator* Gen = CurrentGenerator.Get()) Gen->CancelRepair();
		return;

	case ENLPlayerMode::UsingFuseBox:
		CloseFuseBox();
		return;

	case ENLPlayerMode::Walking:
		if (IsBusy()) return;
		if (AActor* Target = FocusedActor.Get())
		{
			if (INLInteractable* I = Cast<INLInteractable>(Target))
			{
				if (I->CanInteract(this)) I->Interact(this);
			}
		}
		return;

	default:
		return;
	}
}

void ANLPlayerCharacter::OnSpace()
{
	if (Mode == ENLPlayerMode::Seated)
	{
		if (ANLSecurityDesk* Desk = CurrentDesk.Get()) Desk->FlashActiveCamera();
	}
	else if (Mode == ENLPlayerMode::Repairing)
	{
		if (ANLGenerator* Gen = CurrentGenerator.Get()) Gen->HandleRepairInput();
	}
	else if (Mode == ENLPlayerMode::HoldingDoor)
	{
		if (ANLHideDoor* Door = HoldDoor.Get()) Door->HandleHoldInput();
	}
}

void ANLPlayerCharacter::OnFlashlight()
{
	if (Mode != ENLPlayerMode::Dead) Flashlight->Toggle();
}

void ANLPlayerCharacter::OnCameraNext()
{
	if (Mode == ENLPlayerMode::Seated && CurrentDesk.IsValid()) CurrentDesk->NextCamera();
}

void ANLPlayerCharacter::OnCameraPrev()
{
	if (Mode == ENLPlayerMode::Seated && CurrentDesk.IsValid()) CurrentDesk->PreviousCamera();
}

void ANLPlayerCharacter::OnCameraSelect(const FInputActionInstance& Instance)
{
	if (Mode != ENLPlayerMode::Seated || !CurrentDesk.IsValid()) return;

	const UInputAction* Source = Instance.GetSourceAction();
	for (int32 i = 0; i < CameraSelectActions.Num(); ++i)
	{
		if (CameraSelectActions[i].Get() == Source)
		{
			CurrentDesk->SwitchToCamera(i);
			return;
		}
	}
}

// ------------------------------------------------------------------ Tick

void ANLPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Mode == ENLPlayerMode::Walking && !IsBusy())
	{
		UpdateFocus();
		UpdateFootsteps(DeltaSeconds);
	}
	else
	{
		FocusedActor.Reset();
	}

	UpdateStamina(DeltaSeconds);
	if (bDebugWalk) AddMovementInput(GetActorForwardVector(), 1.f);
	UpdateBody(DeltaSeconds);
}

void ANLPlayerCharacter::UpdateFocus()
{
	// Etkileşimli nesneleri birkaç saniyede bir topla
	InteractableCacheTimer -= GetWorld()->GetDeltaSeconds();
	if (InteractableCacheTimer <= 0.f)
	{
		InteractableCacheTimer = 2.f;
		InteractableCache.Reset();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->Implements<UNLInteractable>()) InteractableCache.Add(*It);
		}
	}

	const FVector Eye = FirstPersonCamera->GetComponentLocation();
	const FVector Fwd = (Controller ? Controller->GetControlRotation() : GetActorRotation()).Vector();

	// 1) Artı doğrudan bir etkileşimli nesneye değiyorsa o seçilir
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NLInteract), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Fwd * InteractDistance, ECC_Visibility, Params) &&
		Hit.GetActor() && Hit.GetActor()->Implements<UNLInteractable>())
	{
		const INLInteractable* I = Cast<INLInteractable>(Hit.GetActor());
		if (I && I->CanInteract(this))
		{
			FocusedActor = Hit.GetActor();
			return;
		}
	}

	// 2) Yoksa: menzildeki, artıya en yakın görünen nesne (sınırının içinde ya da biraz dışında)
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<AActor>& Weak : InteractableCache)
	{
		AActor* A = Weak.Get();
		if (!A || A->IsHidden()) continue;
		const INLInteractable* I = Cast<INLInteractable>(A);
		if (!I || !I->CanInteract(this)) continue;

		const FBox Box = A->GetComponentsBoundingBox();
		if (!Box.IsValid) continue;
		// Mesafe: nesnenin sınırına (dibine girince de çalışsın)
		const float Dist = FVector::Dist(Box.GetClosestPointTo(Eye), Eye);
		if (Dist > InteractDistance) continue;

		// Açı: artının nesneye uzaklığı; nesne ne kadar büyük görünüyorsa o kadar pay
		const FVector Center = Box.GetCenter();
		const FVector ToCenter = Center - Eye;
		const float CenterDist = FMath::Max(ToCenter.Size(), 1.f);
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Fwd, ToCenter / CenterDist), -1.f, 1.f)));
		const float Radius = Box.GetExtent().Size() * 0.8f;
		const float Allowed = InteractAimTolerance + FMath::RadiansToDegrees(FMath::Atan(Radius / CenterDist));
		if (Angle > Allowed) continue;

		// Arada duvar var mı?
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(NLInteractSight), false, this);
		SightParams.AddIgnoredActor(A);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, SightParams)) continue;

		const float Score = Angle / Allowed;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = A;
		}
	}
	FocusedActor = Best;
}

void ANLPlayerCharacter::UpdateFootsteps(float DeltaSeconds)
{
	const bool bMoving = GetVelocity().Size2D() > 20.f && GetCharacterMovement()->IsMovingOnGround();
	if (!bMoving)
	{
		FootstepTimer = FootstepInterval * 0.5f;
		return;
	}

	FootstepTimer -= DeltaSeconds;
	if (FootstepTimer <= 0.f)
	{
		// Koşarken adımlar sıklaşır
		FootstepTimer = FootstepInterval * FMath::Clamp(WalkSpeed / FMath::Max(GetVelocity().Size2D(), 1.f), 0.55f, 1.f);
		const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		UGameplayStatics::PlaySoundAtLocation(this, FootstepSound, Feet, FMath::FRandRange(0.8f, 1.f), FMath::FRandRange(0.92f, 1.08f));
	}
}

FText ANLPlayerCharacter::GetCurrentInteractPrompt() const
{
	switch (Mode)
	{
	case ENLPlayerMode::Seated:
		return FText::FromString(TEXT("[SPACE] Light   [A/D] Camera   [1-8] Select camera   [E] Stand up"));
	case ENLPlayerMode::Repairing:
		if (const ANLGenerator* Gen = CurrentGenerator.Get()) return Gen->GetInteractPrompt(this);
		return FText::GetEmpty();
	case ENLPlayerMode::Walking:
		if (AActor* Target = FocusedActor.Get())
		{
			if (const INLInteractable* I = Cast<INLInteractable>(Target)) return I->GetInteractPrompt(this);
		}
		return FText::GetEmpty();
	default:
		return FText::GetEmpty();
	}
}

// ------------------------------------------------------------------ Desk

void ANLPlayerCharacter::SitAtDesk(ANLSecurityDesk* Desk)
{
	if (!Desk || Mode != ENLPlayerMode::Walking) return;

	Mode = ENLPlayerMode::Seated;
	CurrentDesk = Desk;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// Sandalyeye yürüyüp otur: kapsül oturma noktasına kayar, kalça oturağa iner, bakış monitöre döner
	SitPhase = ESitPhase::SittingDown;
	SitTimer = 0.f;
	SitFromLocation = GetActorLocation();
	SitPelvis = Desk->GetSitPelvisLocation();
	SitForward = Desk->GetSeatTransform().GetRotation().GetForwardVector();
	SitFloorZ = Desk->GetFloorZ();
	SitDeskHeight = Desk->GetDeskSurfaceHeight();
	SitLookAt = Desk->GetMonitorLocation();

	PlayBodyMontage(SitMontage);
}

void ANLPlayerCharacter::FinishSitDown()
{
	SitPhase = ESitPhase::Seated;
	SitAlpha = 1.f;
	if (ANLSecurityDesk* Desk = CurrentDesk.Get()) Desk->OnPlayerSeated();
}

void ANLPlayerCharacter::StandUp()
{
	if (Mode != ENLPlayerMode::Seated || SitPhase != ESitPhase::Seated) return;

	SitPhase = ESitPhase::StandingUp;
	SitTimer = 0.f;
	SitFromLocation = GetActorLocation();
	PlayBodyMontage(StandMontage);
	UGameplayStatics::PlaySoundAtLocation(this, StandUpSound, GetActorLocation());

	if (ANLSecurityDesk* Desk = CurrentDesk.Get()) Desk->OnPlayerLeft();
}

void ANLPlayerCharacter::FinishStandUp()
{
	SitPhase = ESitPhase::None;
	SitAlpha = 0.f;
	CurrentDesk.Reset();
	Mode = ENLPlayerMode::Walking;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ANLPlayerCharacter::UpdateBody(float DeltaSeconds)
{
	// --- Oturma / kalkma ilerlemesi ---
	float DeskHands = 0.f, ExtraLean = 0.f;
	if (SitPhase == ESitPhase::SittingDown)
	{
		SitTimer += DeltaSeconds;
		const float T = FMath::Clamp(SitTimer / FMath::Max(SitDuration, 0.1f), 0.f, 1.f);
		SitAlpha = FMath::SmoothStep(0.25f, 1.f, T);  // önce sandalyeye yaklaş, sonra otur
		// İnerken ellerle masaya tutunur
		DeskHands = FMath::SmoothStep(0.3f, 0.6f, T);
		ExtraLean = 14.f * FMath::Sin(PI * FMath::Clamp((T - 0.3f) / 0.7f, 0.f, 1.f));

		if (const ANLSecurityDesk* Desk = CurrentDesk.Get())
		{
			const FTransform Seat = Desk->GetSeatTransform();
			const float MoveT = FMath::SmoothStep(0.f, 0.45f, T);
			SetActorLocation(FMath::Lerp(SitFromLocation, Seat.GetLocation(), MoveT));
		}
		if (T >= 1.f) FinishSitDown();
	}
	else if (SitPhase == ESitPhase::StandingUp)
	{
		// Ellerini masaya bastırıp öne eğilir, kalkar, eller masadan ayrılır, bir adım geri çekilir
		SitTimer += DeltaSeconds;
		const float T = FMath::Clamp(SitTimer / FMath::Max(StandDuration, 0.1f), 0.f, 1.f);
		SitAlpha = 1.f - FMath::SmoothStep(0.22f, 0.8f, T);
		DeskHands = 1.f - FMath::SmoothStep(0.6f, 0.88f, T);
		ExtraLean = 24.f * FMath::Sin(PI * FMath::Clamp(T / 0.75f, 0.f, 1.f));
		const float BackT = FMath::SmoothStep(0.4f, 1.f, T);
		SetActorLocation(SitFromLocation - SitForward.GetSafeNormal2D() * (StandStepBack * BackT));
		if (T >= 1.f) FinishStandUp();
	}

	// Otururken bakış monitöre yumuşakça döner
	if (Mode == ENLPlayerMode::Seated && Controller && SitPhase != ESitPhase::None)
	{
		const FRotator Target = (SitLookAt - FirstPersonCamera->GetComponentLocation()).Rotation();
		const FRotator Cur = Controller->GetControlRotation();
		Controller->SetControlRotation(FMath::RInterpTo(Cur, Target, DeltaSeconds, SitPhase == ESitPhase::StandingUp ? 2.f : 5.f));
	}

	// --- Eğilip alma ---
	float Crouch = 0.f, Reach = 0.f, Grab = 0.f;
	if (PickupTimer >= 0.f)
	{
		PickupTimer += DeltaSeconds;
		const float T = FMath::Clamp(PickupTimer / FMath::Max(PickupDuration, 0.1f), 0.f, 1.f);
		Crouch = FMath::Sin(PI * T);
		Reach = FMath::Clamp(FMath::Sin(PI * T) * 1.4f, 0.f, 1.f);
		Grab = T >= 0.5f ? 1.f : 0.f;
		if (Controller)
		{
			const FRotator Cur = Controller->GetControlRotation();
			const FRotator Target = T < 0.6f ? PickupLookRotation : FRotator(-10.f, PickupLookRotation.Yaw, 0.f);
			Controller->SetControlRotation(FMath::RInterpTo(Cur, Target, DeltaSeconds, 6.f));
		}
		if (T >= 1.f)
		{
			PickupTimer = -1.f;
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	// --- El hareketi (şalter, fener şarjı, benzin dökme) ---
	float ReachR = 0.f, Pour = 0.f;
	FVector ReachTargetR = ActionTarget;
	FTransform PourTransform = FTransform::Identity;
	if (HandAction != ENLHandAction::None)
	{
		ActionTimer += DeltaSeconds;
		const float T = FMath::Clamp(ActionTimer / FMath::Max(ActionDuration, 0.1f), 0.f, 1.f);

		if (!bActionPeakDone && T >= ActionPeak)
		{
			bActionPeakDone = true;
			if (ActionOnPeak) ActionOnPeak();
		}

		// Yaptığı işe bakar
		if (Controller)
		{
			FRotator Look = (ActionTarget - FirstPersonCamera->GetComponentLocation()).Rotation();
			Look.Pitch = FMath::Clamp(Look.Pitch, -60.f, 25.f);
			Look.Roll = 0.f;
			Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Look, DeltaSeconds, 5.f));
		}

		if (HandAction == ENLHandAction::Refuel)
		{
			// Bidonu kaldır, eğ ve dök, düzelt, indir
			Pour = FMath::SmoothStep(0.f, 0.2f, T) * (1.f - FMath::SmoothStep(0.84f, 1.f, T));
			const float Tilt = FMath::SmoothStep(0.22f, 0.4f, T) * (1.f - FMath::SmoothStep(0.68f, 0.84f, T));
			PourTransform = FTransform(FRotator(-80.f * Tilt, ActionRotation.Yaw, 0.f), ActionTarget + FVector(0.f, 0.f, 12.f * Tilt));
		}
		else
		{
			// Uzan, işi yap, eli geri çek
			ReachR = FMath::SmoothStep(0.f, ActionPeak * 0.85f, T) * (1.f - FMath::SmoothStep(FMath::Min(ActionPeak + 0.2f, 0.95f), 1.f, T));
			if (HandAction == ENLHandAction::Switch)
			{
				// Şalter kolunu tutup aşağı/yukarı çeker
				const float Pull = FMath::SmoothStep(ActionPeak - 0.12f, ActionPeak + 0.08f, T);
				ReachTargetR = ActionTarget + FVector(0.f, 0.f, ActionSwitchDir * (Pull * 14.f - 7.f));
			}
		}

		if (T >= 1.f)
		{
			HandAction = ENLHandAction::None;
			ActionOnPeak = nullptr;
			bCanStillVisible = false;
			if (Mode == ENLPlayerMode::Walking) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	// --- Saklanma kapısı: kapıya yaslan, kola bak, nefes nefese ---
	FVector HoldGrip = FVector::ZeroVector, HoldPalm = FVector::ZeroVector;
	ANLHideDoor* Door = HoldDoor.Get();
	const bool bHolding = Mode == ENLPlayerMode::HoldingDoor && Door;
	if (bHolding)
	{
		HoldTimer += DeltaSeconds;
		const float MoveT = FMath::SmoothStep(0.f, 0.55f, HoldTimer);
		SetActorLocation(FMath::Lerp(HoldFromLocation, Door->GetHoldStandLocation(GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), MoveT));
		HoldGrip = Door->GetHandleGripLocation();
		HoldPalm = Door->GetDoorPalmLocation();
	}
	HoldHands = FMath::FInterpTo(HoldHands, bHolding ? 1.f : 0.f, DeltaSeconds, bHolding ? 5.f : 3.f);
	const float HoldReach = bHolding ? HoldHands : 0.f;

	// Panik nefesi: hızlı nefes ritminde bakış iner-kalkar; zorlama anında sarsıntı artar
	const float Force = Door ? Door->GetForceAlpha() : 0.f;
	BreathAlpha = FMath::FInterpTo(BreathAlpha, bHolding ? 1.f : 0.f, DeltaSeconds, bHolding ? 3.f : 0.6f);
	if (Controller && Mode != ENLPlayerMode::Dead && (BreathAlpha > 0.002f || !BreathOffset.IsZero()))
	{
		BreathTime += DeltaSeconds * (1.f + 0.35f * Force);
		const float W = 2.f * PI * BreathRate;
		const float Amp = BreathShake * BreathAlpha * (1.f + 1.6f * Force);
		FRotator NewBreath(
			Amp * (FMath::Sin(BreathTime * W) + 0.25f * FMath::Sin(BreathTime * W * 2.f + 0.7f)),
			Amp * 0.45f * FMath::Sin(BreathTime * W * 0.5f + 1.3f),
			Amp * 0.6f * FMath::Sin(BreathTime * W * 0.33f));
		if (Force > 0.05f)
		{
			NewBreath += FRotator(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.5f, 0.5f)) * (1.4f * Force);
		}
		if (BreathAlpha <= 0.002f) NewBreath = FRotator::ZeroRotator;

		FRotator Base = Controller->GetControlRotation() - BreathOffset;
		if (bHolding)
		{
			// Kamera kendiliğinden kapı koluna döner
			// Kolun biraz üstüne bakar: kol ekranın alt-ortasında, kapı kadrajda
			FRotator Look = (HoldGrip + FVector(0.f, 0.f, 28.f) - FirstPersonCamera->GetComponentLocation()).Rotation();
			Look.Roll = 0.f;
			Base = FMath::RInterpTo(Base, Look, DeltaSeconds, 4.f);
		}
		Controller->SetControlRotation(Base + NewBreath);
		BreathOffset = NewBreath;
	}
	else if (Mode == ENLPlayerMode::Dead)
	{
		BreathOffset = FRotator::ZeroRotator;
	}

	// Elektrik çarpması: bakış titrer, sonra normale döner
	if (Controller && (ShockTime > 0.f || !ShockShakeOffset.IsZero()))
	{
		ShockTime = FMath::Max(0.f, ShockTime - DeltaSeconds);
		const float A = GetShockAlpha();
		const FRotator NewShake = A > 0.f
			? FRotator(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.6f, 0.6f)) * (ShockShake * A)
			: FRotator::ZeroRotator;
		Controller->SetControlRotation(Controller->GetControlRotation() - ShockShakeOffset + NewShake);
		ShockShakeOffset = NewShake;
	}

	FlashAim = FMath::FInterpTo(FlashAim, Flashlight->IsOn() ? 1.f : 0.f, DeltaSeconds, 7.f);
	WorkAlpha = FMath::FInterpTo(WorkAlpha, Mode == ENLPlayerMode::Repairing ? 1.f : 0.f, DeltaSeconds, 6.f);

	// --- Kamera yüksekliği (oturunca ve eğilince iner) ---
	// Kamera gövdenin kafasını takip eder (oturma, eğilme ve yürüme sallanması dahil)
	FVector EyeRel;
	if (BodyRig->GetEyeActorSpace(EyeRel))
	{
		FirstPersonCamera->SetRelativeLocation(FMath::VInterpTo(FirstPersonCamera->GetRelativeLocation(), EyeRel, DeltaSeconds, 25.f));
	}
	else
	{
		const float SeatedEye = (SitPelvis.Z + SeatedEyeAbovePelvis) - GetActorLocation().Z;
		const float Eye = FMath::Lerp(StandEyeHeight, SeatedEye, SitAlpha) - PickupCrouchDepth * 0.8f * Crouch;
		FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, Eye));
	}

	// --- Gövde pozu ---
	FNLBodyPoseInput& In = BodyRig->GetPoseInput();
	const FRotator ViewRot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	In.View = FTransform(ViewRot, FirstPersonCamera->GetComponentLocation());
	In.SitAlpha = SitAlpha;
	In.SitPelvis = SitPelvis;
	In.SitForward = SitForward;
	In.SitFloorZ = SitFloorZ;
	In.SitDeskHeight = SitDeskHeight;
	In.CrouchAlpha = FMath::Max(Crouch, 0.3f * Pour);
	In.CrouchDepth = PickupCrouchDepth;
	In.ReachAlphaL = FMath::Max(Reach, HoldReach);
	In.ReachTargetL = HoldReach > Reach ? HoldPalm : PickupCanTop;
	In.bHoldFlashlight = !Flashlight->IsDocked();
	In.FlashlightAim = FlashAim;
	In.WorkAlpha = WorkAlpha;
	In.ReachAlphaR = FMath::Max(ReachR, HoldReach);
	In.ReachTargetR = HoldReach > ReachR ? HoldGrip : ReachTargetR;
	In.DeskHandsAlpha = DeskHands;
	In.ExtraLean = ExtraLean;
	In.CanOverrideAlpha = Pour;
	In.CanOverride = PourTransform;
	In.bFlashlightRest = bFlashlightRestValid;
	In.FlashlightRest = FlashlightRestTransform;

	BodyRig->SetCanState(bHasFuelCan || bCanStillVisible, Grab > 0.f || PickupTimer < 0.f ? 1.f : 0.f, PickupCanTop);
}

void ANLPlayerCharacter::PlayHandAction(ENLHandAction Type, const FVector& Target, const FRotator& TargetRotation, float Duration, float PeakFraction,
	TFunction<void()> OnPeak, float SwitchDirection)
{
	// Zaten bir hareket varsa beklemeden işi yap
	if (HandAction != ENLHandAction::None || Mode != ENLPlayerMode::Walking)
	{
		if (OnPeak) OnPeak();
		return;
	}

	HandAction = Type;
	ActionTimer = 0.f;
	ActionDuration = FMath::Max(Duration, 0.2f);
	ActionPeak = FMath::Clamp(PeakFraction, 0.05f, 0.95f);
	bActionPeakDone = false;
	ActionOnPeak = MoveTemp(OnPeak);
	ActionTarget = Target;
	ActionRotation = TargetRotation;
	ActionSwitchDir = SwitchDirection;

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (Type == ENLHandAction::Refuel) bCanStillVisible = true;
	if (Type == ENLHandAction::DockFlashlight)
	{
		FlashlightRestTransform = FTransform(TargetRotation, Target);
		bFlashlightRestValid = true;
	}
}

// ------------------------------------------------------------------ Repair

void ANLPlayerCharacter::EnterRepairMode(ANLGenerator* Generator, UAnimMontage* RepairMontage)
{
	if (!Generator) return;

	Mode = ENLPlayerMode::Repairing;
	CurrentGenerator = Generator;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// Jeneratöre dön
	const FVector To = Generator->GetActorLocation() - GetActorLocation();
	if (Controller) Controller->SetControlRotation(FRotator(-15.f, To.Rotation().Yaw, 0.f));

	PlayBodyMontage(RepairMontage);
}

void ANLPlayerCharacter::ExitRepairMode()
{
	if (Mode != ENLPlayerMode::Repairing) return;

	Mode = ENLPlayerMode::Walking;
	CurrentGenerator.Reset();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->StopAllMontages(0.25f);
}

// ------------------------------------------------------------------ Fuel

void ANLPlayerCharacter::PickupFuelCan(float Amount, const FVector& CanTop)
{
	bHasFuelCan = true;
	CarriedFuel = Amount;

	// Eğil, sol elle bidonu al, doğrul
	PickupTimer = 0.f;
	PickupCanTop = CanTop;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	const FVector ToCan = CanTop - FirstPersonCamera->GetComponentLocation();
	PickupLookRotation = FRotator(FMath::Clamp(ToCan.Rotation().Pitch, -70.f, -20.f), ToCan.Rotation().Yaw, 0.f);
}

float ANLPlayerCharacter::ConsumeFuelCan()
{
	const float Amount = bHasFuelCan ? CarriedFuel : 0.f;
	bHasFuelCan = false;
	CarriedFuel = 0.f;
	return Amount;
}

// ------------------------------------------------------------------ Death

void ANLPlayerCharacter::Die()
{
	if (Mode == ENLPlayerMode::Dead) return;

	if (Mode == ENLPlayerMode::Repairing)
	{
		if (ANLGenerator* Gen = CurrentGenerator.Get()) Gen->CancelRepair();
	}

	Mode = ENLPlayerMode::Dead;
	HoldDoor.Reset();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	Flashlight->ForceOff();
	PlayBodyMontage(DeathMontage);
	UGameplayStatics::PlaySound2D(this, DeathSound);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}
}

void ANLPlayerCharacter::LookAtKiller(AActor* Killer)
{
	if (!Killer || !Controller) return;
	const FVector To = Killer->GetActorLocation() - FirstPersonCamera->GetComponentLocation();
	Controller->SetControlRotation(To.Rotation());
}

void ANLPlayerCharacter::PlayBodyMontage(UAnimMontage* Montage)
{
	if (!Montage) return;
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->Montage_Play(Montage);
	}
}

// ------------------------------------------------------------------ Test: poz ekran görüntüleri

void ANLPlayerCharacter::DebugShot(const TCHAR* Name)
{
	UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s"), Name);
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ConsoleCommand(TEXT("HighResShot 1280x720"));
	}
}

void ANLPlayerCharacter::DebugThirdPerson(bool bEnable, const FVector& Offset)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	if (!bEnable)
	{
		PC->SetViewTarget(this);
		return;
	}
	if (!DebugCamera)
	{
		DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
	}
	// Karakterin önünde-yanında durup ona bakan kamera (Offset: ileri, sağ, yukarı)
	const FRotator Yaw(0.f, GetActorRotation().Yaw, 0.f);
	const FVector Focus = GetActorLocation() + FVector(0.f, 0.f, 10.f);
	const FVector CamLoc = Focus + Yaw.RotateVector(Offset);
	DebugCamera->SetActorLocationAndRotation(CamLoc, (Focus - CamLoc).Rotation());
	PC->SetViewTarget(DebugCamera);
}

void ANLPlayerCharacter::NLDebugShots()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};

	// 1) Fener açık, ileri bakış
	At(0.3f, [this]() { if (Controller) Controller->SetControlRotation(FRotator(-8.f, GetActorRotation().Yaw, 0.f)); if (!Flashlight->IsOn()) Flashlight->Toggle(); });
	At(1.6f, [this]() { DebugShot(TEXT("01 flashlight on (first person)")); });
	At(1.9f, [this]() { DebugThirdPerson(true); });
	At(2.3f, [this]() { DebugShot(TEXT("02 flashlight on (third person)")); });
	// 2) Fener kapalı, aşağı bakış: gövde ve bacaklar
	At(2.6f, [this]() { DebugThirdPerson(false); Flashlight->Toggle(); if (Controller) Controller->SetControlRotation(FRotator(-70.f, GetActorRotation().Yaw, 0.f)); });
	At(3.4f, [this]() { DebugShot(TEXT("03 look down, flashlight off")); });

	// 3) Benzin bidonunu alma
	At(3.8f, [this]()
	{
		for (TActorIterator<ANLFuelCan> It(GetWorld()); It; ++It)
		{
			const FVector CanLoc = It->GetActorLocation();
			SetActorLocation(FVector(CanLoc.X + 75.f, CanLoc.Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation(FRotator(-25.f, 180.f, 0.f));
			break;
		}
	});
	At(4.2f, [this]() { DebugThirdPerson(true, FVector(60.f, 230.f, 20.f)); });
	At(4.4f, [this]()
	{
		for (TActorIterator<ANLFuelCan> It(GetWorld()); It; ++It) { It->Interact(this); break; }
	});
	At(4.95f, [this]() { DebugShot(TEXT("04 pickup, crouching (side view)")); });
	At(5.8f, [this]() { DebugThirdPerson(true, FVector(180.f, -80.f, 0.f)); });
	At(6.1f, [this]() { DebugShot(TEXT("05 carrying fuel can (third person)")); });
	At(6.4f, [this]() { DebugThirdPerson(false); if (Controller) Controller->SetControlRotation(FRotator(-55.f, GetActorRotation().Yaw - 25.f, 0.f)); });
	At(7.0f, [this]() { DebugShot(TEXT("06 carrying fuel can (first person)")); });

	// 4) Masaya oturma
	At(7.4f, [this]()
	{
		for (TActorIterator<ANLSecurityDesk> It(GetWorld()); It; ++It)
		{
			const FTransform Seat = It->GetSeatTransform();
			const FVector Back = -Seat.GetRotation().GetForwardVector();
			SetActorLocation(Seat.GetLocation() + Back * 90.f, false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation(FRotator(0.f, Seat.Rotator().Yaw, 0.f));
			SitAtDesk(*It);
			break;
		}
	});
	At(7.5f, [this]() { DebugThirdPerson(true, FVector(40.f, 220.f, 30.f)); });
	At(8.3f, [this]() { DebugShot(TEXT("07 sitting down (side view)")); });
	At(9.5f, [this]() { DebugShot(TEXT("08 seated (side view)")); });
	At(9.8f, [this]() { DebugThirdPerson(true, FVector(-160.f, 120.f, 60.f)); });
	At(10.2f, [this]() { DebugShot(TEXT("09 seated (behind)")); });
	At(10.5f, [this]() { DebugThirdPerson(false); });
	At(11.0f, [this]() { DebugShot(TEXT("10 seated (first person)")); });

	// 5) Masadan kalkma (ellerle masaya bastırarak)
	At(11.4f, [this]() { StandUp(); DebugThirdPerson(true, FVector(40.f, 220.f, 30.f)); });
	At(11.9f, [this]() { DebugShot(TEXT("11 standing up, hands on desk")); });
	At(12.5f, [this]() { DebugShot(TEXT("12 standing up, rising")); });
	At(13.3f, [this]() { DebugShot(TEXT("13 stood up")); });

	// 6) Şalter
	auto GoTo = [this](const FVector& Target, const FVector& Offset)
	{
		const FVector P(Target.X + Offset.X, Target.Y + Offset.Y, GetActorLocation().Z);
		SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation(FRotator(-10.f, (Target - P).Rotation().Yaw, 0.f));
	};
	At(13.6f, [this, GoTo]()
	{
		for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) { GoTo(It->GetComponentsBoundingBox().GetCenter(), FVector(0.f, 80.f, 0.f)); break; }
		DebugThirdPerson(true, FVector(90.f, 200.f, 10.f));
	});
	At(13.9f, [this]() { for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) { It->Interact(this); break; } });
	At(14.45f, [this]() { DebugShot(TEXT("14 switch, hand on lever")); });
	At(14.8f, [this]() { DebugShot(TEXT("15 switch, pulled")); });

	// 7) Feneri şarja takma
	At(15.3f, [this, GoTo]()
	{
		for (TActorIterator<ANLFlashlightCharger> It(GetWorld()); It; ++It) { GoTo(It->GetActorLocation(), FVector(0.f, 75.f, 0.f)); break; }
		DebugThirdPerson(true, FVector(-120.f, -150.f, 80.f));
	});
	At(15.6f, [this]() { for (TActorIterator<ANLFlashlightCharger> It(GetWorld()); It; ++It) { It->Interact(this); break; } });
	At(16.2f, [this]() { DebugShot(TEXT("16 docking flashlight")); });
	At(17.0f, [this]() { DebugShot(TEXT("17 flashlight on charger")); });

	// 8) Benzin dökme
	At(17.3f, [this, GoTo]()
	{
		bHasFuelCan = true;
		CarriedFuel = 1.f;
		for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It) { GoTo(It->GetComponentsBoundingBox().GetCenter(), FVector(0.f, 115.f, 0.f)); break; }
		DebugThirdPerson(true, FVector(100.f, 210.f, 20.f));
	});
	At(17.6f, [this]()
	{
		for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It)
		{
			const FRotator Facing(0.f, (It->GetComponentsBoundingBox().GetCenter() - GetActorLocation()).Rotation().Yaw, 0.f);
			TWeakObjectPtr<ANLPlayerCharacter> WeakThis(this);
			PlayHandAction(ENLHandAction::Refuel, It->GetPourPoint(GetActorLocation()), Facing, 2.8f, 0.6f, [WeakThis]() { if (WeakThis.IsValid()) WeakThis->ConsumeFuelCan(); });
			break;
		}
	});
	At(18.3f, [this]() { DebugShot(TEXT("18 lifting fuel can")); });
	At(19.2f, [this]() { DebugShot(TEXT("19 pouring fuel")); });
	At(19.6f, [this]() { DebugThirdPerson(false); });
	At(19.9f, [this]() { DebugShot(TEXT("20 pouring fuel (first person)")); });
	At(20.8f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugShots2()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Fuse = [this]() -> ANLFuseBox* { for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) return *It; return nullptr; };

	// Şalterin önünde dur, kola bak
	At(0.3f, [this, Fuse]()
	{
		if (ANLFuseBox* F = Fuse())
		{
			const FVector C = F->GetComponentsBoundingBox().GetCenter();
			const FVector P(C.X + 20.f, C.Y + 130.f, GetActorLocation().Z);
			SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation((C - (P + FVector(0.f, 0.f, 60.f))).Rotation());
		}
	});
	At(1.2f, [this]() { DebugShot(TEXT("21 lever ON")); });
	At(1.4f, [Fuse]() { if (ANLFuseBox* F = Fuse()) F->Trip(); });
	At(2.2f, [this]() { DebugShot(TEXT("22 lever after trip (OFF)")); });
	At(2.4f, [this, Fuse]() { if (ANLFuseBox* F = Fuse()) F->Interact(this); });
	At(4.0f, [this]() { DebugShot(TEXT("23 lever switched back ON")); });

	// Yürürken fener (kapalı) tutan el: yarım adım arayla iki kare
	At(4.3f, [this]()
	{
		SetActorLocation(FVector(-1450.f, 220.f, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation(FRotator(-25.f, 0.f, 0.f));
		bDebugWalk = true;
	});
	At(5.6f, [this]() { DebugShot(TEXT("24 walking, flashlight off A")); });
	At(5.9f, [this]() { DebugShot(TEXT("25 walking, flashlight off B")); });
	At(6.1f, [this]() { DebugThirdPerson(true, FVector(160.f, 150.f, 10.f)); });
	At(6.3f, [this]() { DebugShot(TEXT("26 walking (third person)")); });
	At(6.6f, [this]() { bDebugWalk = false; if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugShots3()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Drowned = [this]() -> ANLDrowned* { for (TActorIterator<ANLDrowned> It(GetWorld()); It; ++It) return *It; return nullptr; };
	auto Cam = [this]() -> ANLSecurityCamera* { for (TActorIterator<ANLSecurityCamera> It(GetWorld()); It; ++It) return *It; return nullptr; };

	// Güvenlik kamerasının gözünden bak
	At(0.3f, [this, Cam, Drowned]()
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		ANLSecurityCamera* C = Cam();
		ANLDrowned* D = Drowned();
		if (!PC || !C || !D) return;
		if (!DebugCamera) DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		DebugCamera->SetActorLocationAndRotation(C->GetActorLocation(), C->GetActorRotation());
		PC->SetViewTarget(DebugCamera);
		D->AppearAt(C->GetDrownedSpawnTransform());
	});
	At(1.3f, [this]() { DebugShot(TEXT("31 drowned appears")); });
	At(1.5f, [Cam, Drowned]() { if (ANLSecurityCamera* C = Cam()) C->Flash(); if (ANLDrowned* D = Drowned()) D->PlayCoverFace(); });
	At(1.9f, [this]() { DebugShot(TEXT("32 cover face")); });
	At(2.5f, [this]() { DebugShot(TEXT("33 cover face (later)")); });
	At(2.75f, [Drowned]() { if (ANLDrowned* D = Drowned()) D->PlayFlee(); });
	At(3.1f, [this]() { DebugShot(TEXT("34 flee, turning")); });
	At(3.55f, [this]() { DebugShot(TEXT("35 flee, running")); });
	At(3.85f, [this]() { DebugShot(TEXT("35b flee, running further")); });
	At(4.2f, [Drowned]() { if (ANLDrowned* D = Drowned()) D->Vanish(); });
	At(4.9f, [this]() { DebugShot(TEXT("36 vanished (no shadow left)")); });

	// Jumpscare: oyuncunun gözünden
	At(5.2f, [this, Drowned]()
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC) PC->SetViewTarget(this);
		if (Controller) Controller->SetControlRotation(FRotator(-5.f, GetActorRotation().Yaw, 0.f));
	});
	At(5.7f, [this, Drowned]() { if (ANLDrowned* D = Drowned()) D->PerformJumpscare(Cast<APlayerController>(GetController())); });
	At(5.82f, [this]() { DebugShot(TEXT("37a jumpscare, arms rising")); });
	At(6.15f, [this]() { DebugShot(TEXT("37 jumpscare, grabbing")); });
	At(6.5f, [this]() { DebugShot(TEXT("37b jumpscare, holding")); });
	At(6.7f, [this]() { DebugThirdPerson(true, FVector(40.f, 170.f, 30.f)); });
	At(6.9f, [this]() { DebugShot(TEXT("37c jumpscare (side view)")); });
	At(7.2f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugShots4()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};

	// Kovalamayı başlat, oyuncu biraz aşağı baksın (yakalanınca bakış düzeltilmeli)
	At(0.3f, [this]()
	{
		if (Controller) Controller->SetControlRotation(FRotator(-30.f, GetActorRotation().Yaw, 0.f));
		if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->RequestRunnerChase(this);
	});
	// Koşan Runner'ı yandan görmek için kamerayı ona bağla
	At(1.2f, [this]()
	{
		for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
		{
			APlayerController* PC = Cast<APlayerController>(GetController());
			if (!PC) break;
			if (!DebugCamera) DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
			const FVector R = It->GetActorLocation();
			const FVector Side = FRotationMatrix(It->GetActorRotation()).GetUnitAxis(EAxis::Y);
			const FVector CamLoc = R + Side * 260.f + FVector(0.f, 0.f, 40.f);
			DebugCamera->SetActorLocationAndRotation(CamLoc, (R - CamLoc).Rotation());
			PC->SetViewTarget(DebugCamera);
			break;
		}
	});
	At(1.25f, [this]() { DebugShot(TEXT("41 runner running")); });
	At(1.35f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->SetViewTarget(this); });

	// Yakalandığı anı bekle, sonra kareler al
	FTimerHandle Poll;
	TM.SetTimer(Poll, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		static bool bDone = false;
		if (bDone || Mode != ENLPlayerMode::Dead) return;
		bDone = true;
		FTimerManager& T = GetWorldTimerManager();
		FTimerHandle A, B, C, D, E, Q;
		T.SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugShot(TEXT("42 runner jumpscare, arms rising")); }), 0.12f, false);
		T.SetTimer(B, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugShot(TEXT("43 runner jumpscare, grabbing")); }), 0.45f, false);
		T.SetTimer(C, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(true, FVector(40.f, 170.f, 30.f)); }), 0.8f, false);
		T.SetTimer(D, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugShot(TEXT("44 runner jumpscare (side view)")); }), 1.0f, false);
		T.SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 1.4f, false);
	}), 0.02f, true);
}

void ANLPlayerCharacter::NLDebugShots5()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto UIShot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s"), Name);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	auto LookAt = [this](const FVector& P)
	{
		if (Controller) Controller->SetControlRotation((P - FirstPersonCamera->GetComponentLocation()).Rotation());
	};
	auto FuseCenter = [this]() { for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) return It->GetComponentsBoundingBox().GetCenter(); return FVector::ZeroVector; };
	auto ChargerCenter = [this]() { for (TActorIterator<ANLFlashlightCharger> It(GetWorld()); It; ++It) return It->GetComponentsBoundingBox().GetCenter(); return FVector::ZeroVector; };

	// 1) Uzaktan (5 m): sadece yuvarlaklar
	At(0.4f, [this, FuseCenter, ChargerCenter, LookAt]()
	{
		const FVector Mid = (FuseCenter() + ChargerCenter()) * 0.5f;
		SetActorLocation(FVector(Mid.X, Mid.Y + 480.f, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		LookAt(Mid);
	});
	At(1.6f, [UIShot]() { UIShot(TEXT("51 far: circles only")); });

	// 2) Yakın: şaltere bak -> şalterde E, şarjda sadece yuvarlak
	At(2.0f, [this, FuseCenter, ChargerCenter, LookAt]()
	{
		const FVector Mid = (FuseCenter() + ChargerCenter()) * 0.5f;
		SetActorLocation(FVector(Mid.X, Mid.Y + 170.f, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		LookAt(FuseCenter());
	});
	At(3.0f, [UIShot]() { UIShot(TEXT("52 near, looking at breaker")); });

	// 3) Şarj istasyonuna bak -> E oraya geçer
	At(3.3f, [ChargerCenter, LookAt]() { LookAt(ChargerCenter()); });
	At(4.3f, [UIShot]() { UIShot(TEXT("53 near, looking at charger")); });
	At(4.8f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugRunnerRepair()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Gen = [this]() -> ANLGenerator* { for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It) return *It; return nullptr; };
	auto Runner = [this]() -> ANLRunner* { for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) return *It; return nullptr; };
	auto Report = [this, Runner](const TCHAR* Tag)
	{
		ANLRunner* R = Runner();
		UE_LOG(LogTemp, Log, TEXT("NLTest %s: mode=%d runner_chasing=%d runner_hidden=%d dist=%.0f"), Tag, int32(Mode),
			R ? int32(R->IsChasing()) : -1, R ? int32(R->IsHidden()) : -1, R ? FVector::Dist(R->GetActorLocation(), GetActorLocation()) : -1.f);
	};

	At(0.3f, [this, Gen]() { if (ANLGenerator* G = Gen()) { G->BreakDown(); G->Interact(this); } });
	At(0.6f, [this, Report]()
	{
		Report(TEXT("before mistake"));
		if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->RequestRunnerChase(this);  // tamir hatası
	});
	At(2.0f, [Report]() { Report(TEXT("still repairing (runner must wait)")); });
	At(3.5f, [Report]() { Report(TEXT("still repairing (runner must wait)")); });
	At(3.6f, [this, Gen]() { if (ANLGenerator* G = Gen()) G->CancelRepair(); });
	At(4.0f, [this, Runner, Report]()
	{
		Report(TEXT("left repair (runner should run)"));
		// Runner'dan uzağa kaç
		if (ANLRunner* R = Runner())
		{
			const FVector Away = GetActorLocation() - R->GetActorLocation();
			if (Controller) Controller->SetControlRotation(FRotator(0.f, Away.Rotation().Yaw, 0.f));
		}
		bDebugWalk = true;
	});
	for (int32 i = 1; i <= 6; ++i)
	{
		At(4.0f + i * 1.5f, [Report]() { Report(TEXT("escaping")); });
	}
	At(13.6f, [this]() { bDebugWalk = false; if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugShots6()
{
	// Şalter hemen atmak istesin -> Sabotajcı gelir. Kamera onu yandan takip eder; her aşamada bir kare.
	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->NLDebugFuseIn(0.3f);

	FTimerHandle Poll;
	GetWorldTimerManager().SetTimer(Poll, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		static int32 LastState = -1;
		static float StateClock = 0.f;
		static int32 Shots = 0;
		ANLSaboteur* S = nullptr;
		for (TActorIterator<ANLSaboteur> It(GetWorld()); It; ++It) { S = *It; break; }
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!S || !PC) return;

		const int32 St = S->GetDebugState();
		StateClock = (St == LastState) ? StateClock + 0.1f : 0.f;
		const bool bEntered = St != LastState;
		LastState = St;
		if (St == 0) { if (Shots > 0) PC->ConsoleCommand(TEXT("quit")); return; }

		if (!DebugCamera) DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		const FVector L = S->GetActorLocation();
		const FVector Side = FRotationMatrix(S->GetActorRotation()).GetUnitAxis(EAxis::Y);
		const FVector Fwd = S->GetActorForwardVector();
		// Kurcalarken arkadan-yandan, hedefe doğru bak; diğer aşamalarda yandan
		const FVector CamLoc = St == 2 ? L - Fwd * 170.f + Side * 110.f + FVector(0.f, 0.f, 70.f) : L + Side * 230.f + Fwd * 90.f + FVector(0.f, 0.f, 50.f);
		const FVector Look = St == 2 ? L + Fwd * 60.f + FVector(0.f, 0.f, 20.f) : L;
		DebugCamera->SetActorLocationAndRotation(CamLoc, (Look - CamLoc).Rotation());
		PC->SetViewTarget(DebugCamera);

		// 1: yürürken 1.5 sn sonra, 2: kurcalarken 1 sn sonra, 3: kaçarken 0.8 sn sonra
		const float When = St == 1 ? 1.5f : (St == 2 ? 1.0f : 0.8f);
		if (!bEntered && FMath::IsNearlyEqual(StateClock, When, 0.05f))
		{
			const TCHAR* Names[] = { TEXT(""), TEXT("61 saboteur sneaking"), TEXT("62 saboteur tampering with breaker"), TEXT("63 saboteur running away") };
			DebugShot(Names[St]);
			++Shots;
		}
	}), 0.1f, true);
}

// ------------------------------------------------------------------ Sigorta kutusu

void ANLPlayerCharacter::OpenFuseBox(ANLFuseBox* Box)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!Box || !PC || Mode != ENLPlayerMode::Walking || IsBusy()) return;

	Mode = ENLPlayerMode::UsingFuseBox;
	CurrentFuseBox = Box;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	PC->SetControlRotation((Box->GetComponentsBoundingBox().GetCenter() - FirstPersonCamera->GetComponentLocation()).Rotation());
	// Görüş kutunun yakın plan kamerasına kayar; oyuncunun gövdesi kadraja girmesin
	PC->SetViewTargetWithBlend(Box, 0.45f, VTBlend_EaseInOut, 2.f);
	Box->SetViewing(true);
	// Gövde ve eldeki nesneler gizlenir; fenerin ışığı açık kalır (kutuyu aydınlatır)
	BodyRig->SetHiddenInGame(true);
	FlashlightMesh->SetHiddenInGame(true);
	CarryCanMesh->SetHiddenInGame(true);

	FuseWidget = CreateWidget<UNLFuseBoxWidget>(PC, UNLFuseBoxWidget::StaticClass());
	if (!FuseWidget) { CloseFuseBox(); return; }
	FuseWidget->Init(Box, this);
	FuseWidget->AddToViewport(20);

	FInputModeGameAndUI InputMode;
	// Odak oyun ekranında kalır: fare tıklamaları oyuna gider, katman her karede fareyi okur
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}

void ANLPlayerCharacter::CloseFuseBox()
{
	CloseFuseBoxWithBlend(0.4f);
}

void ANLPlayerCharacter::CloseFuseBoxWithBlend(float BlendTime)
{
	const bool bWasOpen = Mode == ENLPlayerMode::UsingFuseBox;
	if (FuseWidget)
	{
		FuseWidget->RemoveFromParent();
		FuseWidget = nullptr;
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		if (bWasOpen) PC->SetViewTargetWithBlend(this, BlendTime, VTBlend_EaseInOut, 2.f);
	}
	BodyRig->SetHiddenInGame(false);
	FlashlightMesh->SetHiddenInGame(false);
	CarryCanMesh->SetHiddenInGame(false);
	if (ANLFuseBox* OldBox = CurrentFuseBox.Get()) OldBox->SetViewing(false);
	CurrentFuseBox.Reset();
	if (Mode == ENLPlayerMode::UsingFuseBox)
	{
		Mode = ENLPlayerMode::Walking;
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void ANLPlayerCharacter::ElectricShock()
{
	// Kutudan hızla geri at, normal kameraya dön; ekran elektriklenir ve sallanır
	CloseFuseBoxWithBlend(0.08f);
	ShockTime = ShockDuration;
	UGameplayStatics::PlaySound2D(this, ShockSound);
	UE_LOG(LogTemp, Log, TEXT("NLPlayer: electric shock (touched a fuse while the switch was ON)"));
}

void ANLPlayerCharacter::NLDebugFuseUI()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Fuse = [this]() -> ANLFuseBox* { for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) return *It; return nullptr; };
	auto Shot = [this](const TCHAR* Name, bool bUI)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s"), Name);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(bUI ? TEXT("shot showui") : TEXT("HighResShot 1280x720"));
	};

	// Sigorta panelinin önüne geç
	At(0.3f, [this, Fuse]()
	{
		if (ANLFuseBox* F = Fuse())
		{
			const FVector C = F->GetComponentsBoundingBox().GetCenter();
			SetActorLocation(FVector(C.X, C.Y + 150.f, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation((C - FirstPersonCamera->GetComponentLocation()).Rotation());
		}
	});
	At(1.0f, [Shot]() { Shot(TEXT("70 world: fuse panel"), false); });
	At(1.3f, [Fuse]() { if (ANLFuseBox* F = Fuse()) F->SabotageWith(false); });   // sigortalar karışık, şalter açık kaldı
	At(1.9f, [Shot]() { Shot(TEXT("71 world: fuses swapped, switch left ON"), false); });

	At(2.2f, [this, Fuse]() { OpenFuseBox(Fuse()); });
	At(2.8f, [Shot]() { Shot(TEXT("72 UI: switch ON, fuses wrong"), true); });
	At(3.0f, [this]() { if (FuseWidget) FuseWidget->TryPickFuse(0); });           // şalter açıkken dokun -> çarpılır
	At(3.08f, [Shot]() { Shot(TEXT("73 electric shock"), true); });
	At(3.45f, [Shot]() { Shot(TEXT("74 electric shock (fading)"), true); });

	At(4.4f, [this, Fuse]() { OpenFuseBox(Fuse()); });
	At(4.6f, [this]() { if (FuseWidget) FuseWidget->ToggleSwitch(); });           // önce şalteri kapat
	At(5.3f, [Shot]() { Shot(TEXT("75 UI: switch OFF"), true); });
	// Bir sigortayı tutup panelin üstünde hedef yuvaya doğru sürükle (yarı yolda kare al)
	At(5.35f, [this, Fuse]()
	{
		ANLFuseBox* F = Fuse();
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!F || !FuseWidget || !PC) return;
		for (int32 j = 0; j < ANLFuseBox::NumFuses; ++j)
		{
			const int32 Want = F->GetFuseInSlot(j);
			if (Want == j) continue;
			if (FuseWidget->TryPickFuse(j))
			{
				const FVector Mid = (F->GetSlotWorldLocation(j) + F->GetSlotWorldLocation(Want)) * 0.5f;
				const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
				F->UpdateDrag(Cam, (Mid - Cam).GetSafeNormal());
			}
			break;
		}
	});
	At(5.45f, [Shot]() { Shot(TEXT("75b dragging a fuse on the panel"), true); });
	At(5.6f, [this, Fuse]()
	{
		ANLFuseBox* F = Fuse();
		if (!F || !FuseWidget) return;
		FuseWidget->DropFuse(INDEX_NONE);
		for (int32 i = 0; i < ANLFuseBox::NumFuses; ++i)
		{
			if (F->GetFuseInSlot(i) == i) continue;
			for (int32 j = 0; j < ANLFuseBox::NumFuses; ++j)
			{
				if (F->GetFuseInSlot(j) == i && FuseWidget->TryPickFuse(j)) { FuseWidget->DropFuse(i); break; }
			}
		}
	});
	At(6.0f, [Shot]() { Shot(TEXT("76 UI: fuses fixed, switch still OFF"), true); });
	At(6.2f, [this]() { if (FuseWidget) FuseWidget->ToggleSwitch(); });           // şalteri aç
	At(6.9f, [Shot]() { Shot(TEXT("77 UI: power restored"), true); });
	At(7.1f, [this]() { CloseFuseBox(); });
	At(7.7f, [Shot]() { Shot(TEXT("78 world: fixed"), false); });
	At(8.2f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugFuseMouse()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Fuse = [this]() -> ANLFuseBox* { for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) return *It; return nullptr; };
	auto PCOf = [this]() { return Cast<APlayerController>(GetController()); };
	// Sol tuşu gerçek giriş gibi bas / bırak
	auto Mouse = [PCOf](EInputEvent Ev)
	{
		APlayerController* PC = PCOf();
		if (!PC || !GEngine || !GEngine->GameViewport) return;
		FInputKeyEventArgs Args(GEngine->GameViewport->Viewport, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(), EKeys::LeftMouseButton, Ev, FPlatformTime::Cycles64());
		PC->InputKey(Args);
	};
	auto MoveTo = [PCOf](const FVector& World, const FVector2D& Offset)
	{
		APlayerController* PC = PCOf();
		FVector2D S;
		if (PC && PC->ProjectWorldLocationToScreen(World, S, true)) PC->SetMouseLocation(int32(S.X + Offset.X), int32(S.Y + Offset.Y));
	};
	auto Shot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s"), Name);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	auto Report = [Fuse](const TCHAR* Tag)
	{
		if (ANLFuseBox* F = Fuse())
			UE_LOG(LogTemp, Log, TEXT("NLTest %s: slots=[%d %d %d %d] switch=%s"), Tag, F->GetFuseInSlot(0), F->GetFuseInSlot(1), F->GetFuseInSlot(2), F->GetFuseInSlot(3), F->IsOn() ? TEXT("ON") : TEXT("OFF"));
	};
	static int32 From = 0, To = 1;

	At(1.20f, [this, Fuse]()
	{
		ANLFuseBox* F = Fuse();
		if (!F) return;
		const FVector C = F->GetComponentsBoundingBox().GetCenter();
		SetActorLocation(FVector(C.X, C.Y + 150.f, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		F->SabotageWith(true);   // sigortalar karışık, şalter kapalı
		OpenFuseBox(F);
	});
	At(4.00f, [Report]() { Report(TEXT("before drag")); });

	// 1) Yanlış yuvadaki bir sigortayı tut, kendi yuvasına sürükle, bırak
	At(4.40f, [Fuse, MoveTo, Mouse]()
	{
		ANLFuseBox* F = Fuse();
		if (!F) return;
		for (int32 j = 0; j < ANLFuseBox::NumFuses; ++j) { if (F->GetFuseInSlot(j) != j) { From = j; To = F->GetFuseInSlot(j); break; } }
		MoveTo(F->GetSlotWorldLocation(From), FVector2D::ZeroVector);
	});
	At(4.80f, [Mouse]() { Mouse(IE_Pressed); });
	for (int32 i = 1; i <= 8; ++i)
	{
		At(4.80f + i * 0.24f, [Fuse, MoveTo, i]()
		{
			if (ANLFuseBox* F = Fuse()) MoveTo(FMath::Lerp(F->GetSlotWorldLocation(From), F->GetSlotWorldLocation(To), i / 8.f), FVector2D::ZeroVector);
		});
	}
	At(5.80f, [Shot]() { Shot(TEXT("81 mouse: dragging a fuse")); });
	At(7.20f, [Mouse]() { Mouse(IE_Released); });
	At(8.00f, [Report]() { Report(TEXT("after drag (one fuse should be home)")); });

	// 2) Sadece tıklamak bir şey yapmamalı ve takılı kalmamalı
	At(8.40f, [Fuse, MoveTo]() { if (ANLFuseBox* F = Fuse()) MoveTo(F->GetSlotWorldLocation(3), FVector2D::ZeroVector); });
	At(8.80f, [Mouse]() { Mouse(IE_Pressed); });
	At(9.20f, [Mouse]() { Mouse(IE_Released); });
	At(9.60f, [Report]() { Report(TEXT("after a plain click (must be unchanged)")); });

	// 3) Şalter koluna basılı tut, yukarı çek, bırak
	At(10.00f, [Fuse, MoveTo]() { if (ANLFuseBox* F = Fuse()) MoveTo(F->GetLeverWorldLocation(), FVector2D::ZeroVector); });
	At(10.40f, [Mouse]() { Mouse(IE_Pressed); });
	for (int32 i = 1; i <= 8; ++i)
	{
		At(10.40f + i * 0.20f, [Fuse, MoveTo, i]() { if (ANLFuseBox* F = Fuse()) MoveTo(F->GetLeverWorldLocation(), FVector2D(0.f, -40.f * i)); });
	}
	At(11.40f, [Shot]() { Shot(TEXT("82 mouse: dragging the lever up")); });
	At(12.40f, [Mouse]() { Mouse(IE_Released); });
	At(14.40f, [Report]() { Report(TEXT("after lever drag up (switch should be ON)")); });
	At(15.20f, [Shot]() { Shot(TEXT("83 after lever drag")); });
	At(17.20f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugFocusShots()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Place = [this](const FVector& Target, const FVector& Offset, const FVector& AimOffset)
	{
		SetActorLocation(FVector(Target.X + Offset.X, Target.Y + Offset.Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation(((Target + AimOffset) - FirstPersonCamera->GetComponentLocation()).Rotation());
	};
	auto Shot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s -> focused: %s"), Name, *GetNameSafe(FocusedActor.Get()));
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	auto Center = [this](UClass* Cls) { for (TActorIterator<AActor> It(GetWorld(), Cls); It; ++It) return It->GetComponentsBoundingBox().GetCenter(); return FVector::ZeroVector; };

	// 1) Şarj istasyonunun dibinde (60 cm), ona bakarken
	At(0.5f, [Place, Center]() { Place(Center(ANLFlashlightCharger::StaticClass()), FVector(0.f, 60.f, 0.f), FVector::ZeroVector); });
	At(2.5f, [Shot]() { Shot(TEXT("91 right next to the charger")); });
	// 2) Jeneratöre yakın (1.5 m), artı jeneratörün biraz yanında
	At(3.5f, [Place, Center]() { Place(Center(ANLGenerator::StaticClass()), FVector(0.f, 150.f, 0.f), FVector(70.f, 0.f, 40.f)); });
	At(5.5f, [Shot]() { Shot(TEXT("92 near the generator, crosshair slightly off")); });
	// 3) Boşluğa bak: odak yok, artı küçük
	At(6.5f, [this]() { if (Controller) Controller->SetControlRotation(FRotator(10.f, GetActorRotation().Yaw + 180.f, 0.f)); });
	At(8.5f, [Shot]() { Shot(TEXT("93 looking at nothing")); });
	At(9.5f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugLook(float X, float Y, float Pitch, float Yaw)
{
	SetActorLocation(FVector(X, Y, GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
	if (Controller) Controller->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("HighResShot 1280x720"));
	}), 1.5f, false);
}

// ------------------------------------------------------------------ Saklanma kapısı

void ANLPlayerCharacter::EnterDoorHold(ANLHideDoor* Door)
{
	if (!Door || Mode != ENLPlayerMode::Walking) return;
	Mode = ENLPlayerMode::HoldingDoor;
	HoldDoor = Door;
	HoldTimer = 0.f;
	HoldFromLocation = GetActorLocation();
	FocusedActor.Reset();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

void ANLPlayerCharacter::ExitDoorHold()
{
	if (Mode != ENLPlayerMode::HoldingDoor) return;
	Mode = ENLPlayerMode::Walking;
	HoldDoor.Reset();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ANLPlayerCharacter::NLDebugDoor(int32 Win)
{
	ANLHideDoor* Door = nullptr;
	for (TActorIterator<ANLHideDoor> It(GetWorld()); It; ++It) { Door = *It; break; }
	if (!Door) { UE_LOG(LogTemp, Warning, TEXT("NLDebugDoor: no ANLHideDoor in the level")); return; }

	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s"), Name);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	const FTransform DT = Door->GetActorTransform();
	const float Z = GetActorLocation().Z;

	// 1) Koridordan açık kapıya bak
	At(0.5f, [this, DT, Z]()
	{
		const FVector P = DT.TransformPosition(FVector(-330.f, -60.f, 0.f));
		SetActorLocation(FVector(P.X, P.Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation((DT.TransformPosition(FVector(0.f, 0.f, 120.f)) - FirstPersonCamera->GetComponentLocation()).Rotation());
	});
	At(3.f, [Shot]() { Shot(TEXT("101 open door from the hallway")); });
	// 2) Odanın içine geç, Runner kovalıyor, kapıyı kapat (E)
	At(4.f, [this, DT, Z]()
	{
		const FVector P = DT.TransformPosition(FVector(150.f, 10.f, 0.f));
		SetActorLocation(FVector(P.X, P.Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation(FRotator(-5.f, DT.Rotator().Yaw + 180.f + 25.f, 0.f));
		if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->RequestRunnerChase(this);
		if (!Flashlight->IsOn()) Flashlight->Toggle();
	});
	At(6.f, [this, Door]() { if (Door->IsOpen()) Door->Interact(this); });
	At(6.4f, [Shot]() { Shot(TEXT("102 door closing")); });
	At(7.8f, [Shot]() { Shot(TEXT("103 camera turned to the handle, panting")); });

	FTimerHandle Poll;
	TM.SetTimer(Poll, FTimerDelegate::CreateWeakLambda(this, [this, Door, Win, Shot]()
	{
		static int32 State = 0;
		if (State == 0 && Mode == ENLPlayerMode::HoldingDoor)
		{
			State = 1;
			UE_LOG(LogTemp, Log, TEXT("NLDebugDoor: hold started"));
		}
		if (State == 1 && Door->IsCheckWindowOpen() && Door->GetNeedlePosition() > 0.15f)
		{
			State = 2;
			Shot(TEXT("104 runner forcing the door, handle down, SPACE check"));
			if (Win) Door->SetDebugAutoResult(1);
			FTimerHandle A, B;
			GetWorldTimerManager().SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(true, FVector(-10.f, 190.f, 45.f)); }), 0.05f, false);
			GetWorldTimerManager().SetTimer(B, FTimerDelegate::CreateWeakLambda(this, [this, Shot]() { Shot(TEXT("104b side view: holding the door")); }), 0.4f, false);
			FTimerHandle C;
			GetWorldTimerManager().SetTimer(C, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(false); }), 0.6f, false);
		}
		if (State == 2 && Win) Door->SetDebugAutoResult(1);   // düşük FPS'te ibre bölgeyi atlamasın diye kapı kendisi basar
		if (State == 2 && !Win && Mode == ENLPlayerMode::Dead)
		{
			State = 3;
			FTimerManager& T = GetWorldTimerManager();
			FTimerHandle A, B, Q;
			T.SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("105 door burst open, jumpscare")); }), 0.25f, false);
			T.SetTimer(B, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("106 jumpscare grab")); }), 0.7f, false);
			FTimerHandle S1, S2;
			T.SetTimer(S1, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(true, FVector(-40.f, -200.f, 80.f)); }), 1.0f, false);
			T.SetTimer(S2, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("107 jumpscare side view")); }), 1.3f, false);
			T.SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 1.9f, false);
		}
		if (State == 2 && Win && Mode == ENLPlayerMode::Walking)
		{
			State = 3;
			FTimerManager& T = GetWorldTimerManager();
			FTimerHandle A, Q;
			T.SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("105 runner left, player free")); }), 1.0f, false);
			T.SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 2.5f, false);
		}
	}), 0.01f, true);
	At(80.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

// ------------------------------------------------------------------ Koşma / stamina

void ANLPlayerCharacter::UpdateStamina(float DeltaSeconds)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bShift = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	const bool bMoving = GetVelocity().Size2D() > 60.f && GetCharacterMovement()->IsMovingOnGround();
	const bool bCanSprint = Mode == ENLPlayerMode::Walking && !IsBusy() && !bExhausted && Stamina > 0.f;
	bSprinting = bShift && bMoving && bCanSprint;

	if (bSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - DeltaSeconds / FMath::Max(SprintDuration, 0.1f));
		StaminaRegenTimer = StaminaRegenDelay;
		if (Stamina <= 0.f)
		{
			bExhausted = true;   // tükendi: bir süre koşamaz
			bDepleted = true;    // tamamen dolana kadar yavaş dolar (bar soluk gri)
			bSprinting = false;
		}
	}
	else
	{
		StaminaRegenTimer -= DeltaSeconds;
		if (StaminaRegenTimer <= 0.f)
		{
			// Dururken daha hızlı dolar; sıfırlandıysa daha yavaş
			float Rate = (bMoving ? 1.f : 1.5f) / FMath::Max(StaminaRegenTime, 0.1f);
			if (bDepleted) Rate *= DepletedRegenMultiplier;
			Stamina = FMath::Min(1.f, Stamina + DeltaSeconds * Rate);
		}
		if (bExhausted && Stamina >= ExhaustedRecover) bExhausted = false;
	}
	if (bDepleted && Stamina >= 0.999f) bDepleted = false;

	if (Mode == ENLPlayerMode::Walking || Mode == ENLPlayerMode::HoldingDoor)
	{
		const float Target = bSprinting ? SprintSpeed : WalkSpeed;
		UCharacterMovementComponent* Move = GetCharacterMovement();
		Move->MaxWalkSpeed = FMath::FInterpTo(Move->MaxWalkSpeed, Target, DeltaSeconds, bSprinting ? 5.f : 3.f);
	}

	// Nefes nefese: stamina azaldıkça duyulur (kapıyı tutarken kapının kendi nefesi çalar)
	const float WantPant = (Mode == ENLPlayerMode::Dead || Mode == ENLPlayerMode::HoldingDoor) ? 0.f
		: FMath::Clamp((0.7f - Stamina) / 0.6f, 0.f, 1.f) * 0.75f * (1.f - ExhaustAlpha);
	PantingVolume = FMath::FInterpTo(PantingVolume, WantPant, DeltaSeconds, 1.5f);
	if (PantingLoop)
	{
		if (PantingVolume > 0.02f)
		{
			if (!PantingAudio->IsPlaying()) PantingAudio->Play();
			PantingAudio->SetVolumeMultiplier(PantingVolume * 0.8f);
		}
		else if (PantingAudio->IsPlaying())
		{
			PantingAudio->Stop();
		}
	}

	// --- Tükenme: derin nefes + ekran nefesle birlikte gri-normal atar ---
	const bool bExhaustFx = bExhausted && Mode != ENLPlayerMode::Dead;
	if (bExhaustFx && !bWasExhausted)
	{
		// Nefes sesi ve griliği aynı anda başlat (senkron)
		ExhaustPhase = 0.f;
		if (ExhaustedBreathLoop)
		{
			ExhaustedBreathAudio->Stop();
			ExhaustedBreathAudio->Play();
		}
	}
	bWasExhausted = bExhaustFx;
	ExhaustAlpha = FMath::FInterpTo(ExhaustAlpha, bExhaustFx ? 1.f : 0.f, DeltaSeconds, bExhaustFx ? 6.f : 1.2f);
	ExhaustPhase = FMath::Fmod(ExhaustPhase + DeltaSeconds / FMath::Max(ExhaustedBreathPeriod, 0.2f), 1.f);
	if (ExhaustedBreathLoop)
	{
		if (ExhaustAlpha > 0.02f)
		{
			if (!ExhaustedBreathAudio->IsPlaying()) ExhaustedBreathAudio->Play();
			ExhaustedBreathAudio->SetVolumeMultiplier(ExhaustAlpha);
		}
		else if (ExhaustedBreathAudio->IsPlaying())
		{
			ExhaustedBreathAudio->Stop();
		}
	}

	// İçe çekişte (nefesin başı) gri, verişte normale döner
	const float Gray = ExhaustAlpha * FMath::SmoothStep(0.f, 1.f, 0.5f + 0.5f * FMath::Cos(2.f * PI * (ExhaustPhase - 0.18f)));
	const float Sat = FMath::Lerp(1.f, ExhaustedSaturation, Gray);
	if (FMath::Abs(Sat - AppliedSaturation) > 0.003f)
	{
		AppliedSaturation = Sat;
		FPostProcessSettings& PP = FirstPersonCamera->PostProcessSettings;
		PP.bOverride_ColorSaturation = Sat < 0.999f;
		PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
		// Gri anlarda hafif kararma da olsun ki fark belli olsun
		PP.bOverride_ColorGain = Sat < 0.999f;
		const float Gain = FMath::Lerp(1.f, 0.82f, Gray);
		PP.ColorGain = FVector4(Gain, Gain, Gain, 1.f);
		FirstPersonCamera->PostProcessBlendWeight = 1.f;
	}
}

void ANLPlayerCharacter::NLDebugStamina()
{
	// Gösterge: dolu (gizli), koşarken, azalmış, tükenmiş halleri
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s stamina=%.2f sprint=%d exhausted=%d speed=%.0f"), Name, Stamina, bSprinting ? 1 : 0, bExhausted ? 1 : 0, GetVelocity().Size2D());
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	// Gerçek Shift tuşu: ileri yürürken bas, hız ve stamina loglanır
	auto Shift = [this](EInputEvent Ev)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC || !GEngine || !GEngine->GameViewport) return;
		FInputKeyEventArgs Args(GEngine->GameViewport->Viewport, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(), EKeys::LeftShift, Ev, FPlatformTime::Cycles64());
		PC->InputKey(Args);
	};
	At(0.3f, [this]() { bDebugWalk = true; });
	At(1.3f, [this]() { UE_LOG(LogTemp, Log, TEXT("NLDebugStamina: walking speed=%.0f stamina=%.2f"), GetVelocity().Size2D(), Stamina); });
	At(1.4f, [Shift]() { Shift(IE_Pressed); });
	At(2.9f, [this]() { UE_LOG(LogTemp, Log, TEXT("NLDebugStamina: sprinting=%d speed=%.0f stamina=%.2f"), bSprinting ? 1 : 0, GetVelocity().Size2D(), Stamina); });
	At(3.0f, [this, Shift]() { Shift(IE_Released); bDebugWalk = false; });
	At(3.5f, [this]() { Stamina = 0.75f; StaminaRegenTimer = 100.f; });
	At(5.5f, [Shot]() { Shot(TEXT("111 stamina 75%")); });
	At(6.f, [this]() { Stamina = 0.35f; });
	At(8.f, [Shot]() { Shot(TEXT("112 stamina 35%")); });
	At(8.5f, [this]() { Stamina = 0.f; bExhausted = true; });
	At(10.5f, [Shot]() { Shot(TEXT("113 exhausted")); });
	// Gri tepe (faz ~0.18) ve normal an (faz ~0.68)
	FTimerHandle GrayPoll;
	TM.SetTimer(GrayPoll, FTimerDelegate::CreateWeakLambda(this, [this, Shot]()
	{
		static int32 Step = 0;
		if (!bExhausted || GetWorld()->GetTimeSeconds() < 11.f) return;
		if (Step == 0 && ExhaustPhase > 0.16f && ExhaustPhase < 0.3f) { Step = 1; Shot(TEXT("114 exhausted, gray")); }
		else if (Step == 1 && ExhaustPhase > 0.62f && ExhaustPhase < 0.78f) { Step = 2; Shot(TEXT("115 exhausted, normal")); }
	}), 0.01f, true);
	// Sıfırlanmış (soluk gri, yavaş dolan) ile normal dolumu karşılaştır
	At(14.f, [this]() { Stamina = 0.6f; bExhausted = false; bDepleted = true; StaminaRegenTimer = 100.f; });
	At(16.f, [Shot]() { Shot(TEXT("116 depleted 60%")); });
	static float RegenStart = 0.f;
	At(16.2f, [this]() { StaminaRegenTimer = 0.f; RegenStart = Stamina; });
	At(17.2f, [this]() { UE_LOG(LogTemp, Log, TEXT("NLDebugStamina: depleted regen per sec=%.3f"), Stamina - RegenStart); Stamina = 0.6f; bDepleted = false; RegenStart = Stamina; });
	At(18.2f, [this]() { UE_LOG(LogTemp, Log, TEXT("NLDebugStamina: normal regen per sec=%.3f"), Stamina - RegenStart); Stamina = 0.6f; StaminaRegenTimer = 100.f; });
	At(20.f, [Shot]() { Shot(TEXT("117 normal 60%")); });
	At(20.5f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}
