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
#include "NLFlashlightCharger.h"
#include "NLDrowned.h"
#include "NLSecurityCamera.h"
#include "NLRunner.h"
#include "NLGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ANLPlayerCharacter::ANLPlayerCharacter()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(FootstepSound, "footstep");
	NL_DEFAULT_SOUND(StandUpSound, "chair_stand");
	NL_DEFAULT_SOUND(DeathSound, "player_death");

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
	if (Mode == ENLPlayerMode::Seated || Mode == ENLPlayerMode::Dead || IsBusy()) return;

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

	if (bDebugWalk) AddMovementInput(GetActorForwardVector(), 1.f);
	UpdateBody(DeltaSeconds);
}

void ANLPlayerCharacter::UpdateFocus()
{
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + FirstPersonCamera->GetForwardVector() * InteractDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NLInteract), false, this);

	AActor* NewFocus = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (Hit.GetActor() && Hit.GetActor()->Implements<UNLInteractable>())
		{
			NewFocus = Hit.GetActor();
		}
	}
	FocusedActor = NewFocus;
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
		FootstepTimer = FootstepInterval;
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
	In.ReachAlphaL = Reach;
	In.ReachTargetL = PickupCanTop;
	In.bHoldFlashlight = !Flashlight->IsDocked();
	In.FlashlightAim = FlashAim;
	In.WorkAlpha = WorkAlpha;
	In.ReachAlphaR = ReachR;
	In.ReachTargetR = ReachTargetR;
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
