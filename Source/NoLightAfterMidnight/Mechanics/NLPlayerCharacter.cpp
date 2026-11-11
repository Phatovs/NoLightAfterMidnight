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

ANLPlayerCharacter::ANLPlayerCharacter()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(FootstepSound, "footstep");

	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(40.f, 92.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	Flashlight = CreateDefaultSubobject<UNLFlashlightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FirstPersonCamera);
	Flashlight->SetRelativeLocation(FVector(10.f, 15.f, -10.f));

	bUseControllerRotationYaw = true;

	// Varsayılan: arayüzü tamamen C++'ta kuran HUD. İsterseniz kendi WBP'nizi atayabilirsiniz.
	HUDWidgetClass = UNLHUDWidget::StaticClass();
}

void ANLPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
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
}

void ANLPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	if (Mode != ENLPlayerMode::Walking || !Controller) return;

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.X);
}

void ANLPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	if (Mode == ENLPlayerMode::Seated || Mode == ENLPlayerMode::Dead) return;

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

	if (Mode == ENLPlayerMode::Walking)
	{
		UpdateFocus();
		UpdateFootsteps(DeltaSeconds);
	}
	else
	{
		FocusedActor.Reset();
	}
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
		return FText::FromString(TEXT("[SPACE] Işık   [A/D] Kamera   [1-8] Kamera seç   [E] Kalk"));
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

	const FTransform Seat = Desk->GetSeatTransform();
	SetActorLocationAndRotation(Seat.GetLocation(), FRotator(0.f, Seat.Rotator().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);

	PlayBodyMontage(SitMontage);
	Desk->OnPlayerSeated();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(Desk, Desk->GetViewBlendTime(), VTBlend_EaseInOut, 2.f);
	}
}

void ANLPlayerCharacter::StandUp()
{
	if (Mode != ENLPlayerMode::Seated) return;

	ANLSecurityDesk* Desk = CurrentDesk.Get();
	Mode = ENLPlayerMode::Walking;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	PlayBodyMontage(StandMontage);
	UGameplayStatics::PlaySoundAtLocation(this, StandUpSound, GetActorLocation());

	if (Desk) Desk->OnPlayerLeft();
	CurrentDesk.Reset();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(this, Desk ? Desk->GetViewBlendTime() : 0.5f, VTBlend_EaseInOut, 2.f);
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

void ANLPlayerCharacter::PickupFuelCan(float Amount)
{
	bHasFuelCan = true;
	CarriedFuel = Amount;
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
