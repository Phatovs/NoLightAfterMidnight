#include "NLPlayerCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "NLGameSubsystem.h"
#include "Containers/Ticker.h"
#include "NLMenuWidgets.h"
#include "NLFuseBox.h"
#include "NLFuseBoxWidget.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Engine/GameViewportClient.h"
#include "NLFlashlightCharger.h"
#include "NLGuideLanterns.h"
#include "NLLighthouse.h"
#include "NLSwingDoor.h"
#include "RHI.h"
#include "Containers/Ticker.h"
#include "Components/PointLightComponent.h"
#include "NLDrowned.h"
#include "NLSecurityCamera.h"
#include "NLRunner.h"
#include "NLSaboteur.h"
#include "NLHideDoor.h"
#include "NLGameMode.h"
#include "NLRunnerAmbushZone.h"
#include "NLDrownedDirector.h"
#include "NLAmbienceDirector.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
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
	// Kitteki eski tip metal fener (orijin arka kapak, +X'e bakar); yoksa düz silindir
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> FlashModel(TEXT("/Game/Environment/Kit/SM_NL_Flashlight.SM_NL_Flashlight"));
	if (FlashModel.Succeeded())
	{
		FlashlightMesh->SetStaticMesh(FlashModel.Get());
		FlashlightMesh->SetRelativeLocation(FVector(-10.f, 0.f, 0.f));
		FlashlightMesh->SetRelativeScale3D(FVector(1.35f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMetal(TEXT("/Game/Environment/Materials/MI_NL_Metal.MI_NL_Metal"));
		if (FlashMetal.Succeeded()) FlashlightMesh->SetMaterial(0, FlashMetal.Object);
		FlashlightMesh->SetRelativeLocationAndRotation(FVector(8.f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
		FlashlightMesh->SetRelativeScale3D(FVector(0.045f, 0.045f, 0.22f));
	}
	FlashlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashlightMesh->SetCastShadow(false);
	// Hareketli küçük obje: uzaklık alanı / Lumen sahnesine girmesin (her kare güncelleme maliyeti)
	FlashlightMesh->bAffectDistanceFieldLighting = false;
	FlashlightMesh->bAffectDynamicIndirectLighting = false;
	FlashlightMesh->bVisibleInRayTracing = false;

	Flashlight = CreateDefaultSubobject<UNLFlashlightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FlashlightPivot);
	Flashlight->SetRelativeLocation(FVector(21.f, 0.f, 0.f));

	// Taşınan benzin bidonu (sol elde)
	CarryCanPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CarryCanPivot"));
	CarryCanPivot->SetupAttachment(GetCapsuleComponent());

	CarryCanMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarryCanMesh"));
	CarryCanMesh->SetupAttachment(CarryCanPivot);
	// Hareketli küçük obje: uzaklık alanı / Lumen sahnesine girmesin (her kare güncelleme maliyeti)
	CarryCanMesh->bAffectDistanceFieldLighting = false;
	CarryCanMesh->bAffectDynamicIndirectLighting = false;
	CarryCanMesh->bVisibleInRayTracing = false;
	CarryCanMesh->SetStaticMesh(CylinderFinder.Get());
	CarryCanMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	CarryCanMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.36f));
	CarryCanMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarryCanMesh->SetCastShadow(false);
	// Benzin bidonu modeli varsa: sapından tutulur
	{
		static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> JerryFinder(TEXT("/Game/Environment/Kit/SM_NL_JerryCan.SM_NL_JerryCan"));
		if (JerryFinder.Get())
		{
			CarryCanMesh->SetStaticMesh(JerryFinder.Get());
			CarryCanMesh->SetRelativeLocation(FVector(0.f, 0.f, -37.f));
			CarryCanMesh->SetRelativeScale3D(FVector(0.75f));
		}
	}
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
	RestoreGameInput();
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

	// ESC (editörde ESC oyunu durdurur: P de çalışır) - oyun dururken de tetiklenir
	PauseAction = MakeAction(TEXT("IA_Pause"), false);
	PauseAction->bTriggerWhenPaused = true;
	MappingContext->MapKey(PauseAction, EKeys::Escape);
	MappingContext->MapKey(PauseAction, EKeys::P);

	// G: elindeki benzin bidonunu yere bırak
	DropAction = MakeAction(TEXT("IA_DropCan"), false);
	MappingContext->MapKey(DropAction, EKeys::G);

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
	EIC->BindAction(DropAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnDrop);
	EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &ANLPlayerCharacter::OnPause);
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

	const UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this);
	const UNLSettingsSave* S = Sub ? Sub->GetSettings() : nullptr;
	const FVector2D Axis = Value.Get<FVector2D>() * MouseSensitivity * (S ? S->MouseSensitivity : 1.f);
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput((S && S->bInvertY) ? Axis.Y : -Axis.Y);
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
	// Ayarlar: görüş açısı
	if (const UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this))
	{
		if (const UNLSettingsSave* S = Sub->GetSettings())
		{
			if (!FMath::IsNearlyEqual(FirstPersonCamera->FieldOfView, S->FieldOfView)) FirstPersonCamera->SetFieldOfView(S->FieldOfView);
		}
	}
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
		Grab = bDroppingCan ? (T < 0.5f ? 1.f : 0.f) : (T >= 0.5f ? 1.f : 0.f);
		// Bırakırken: eğilmenin dibinde bidon yere konur
		if (bDroppingCan && T >= 0.5f && !bDropDone)
		{
			bDropDone = true;
			if (ANLFuelCan* Src = CanSource.Get()) Src->DropAt(DropSpot, GetActorRotation().Yaw + 90.f, CarriedFuel);
			bHasFuelCan = false;
			CarriedFuel = 0.f;
		}
		if (Controller)
		{
			const FRotator Cur = Controller->GetControlRotation();
			const FRotator Target = T < 0.6f ? PickupLookRotation : FRotator(-10.f, PickupLookRotation.Yaw, 0.f);
			Controller->SetControlRotation(FMath::RInterpTo(Cur, Target, DeltaSeconds, 6.f));
		}
		if (T >= 1.f)
		{
			PickupTimer = -1.f;
			bDroppingCan = false;
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
		// Tüm gövdesiyle kapıya yüklenir; Runner zorladıkça gövde geri itilir, tekrar yaslanır
		const float Push = Door->GetForceAlpha();
		const FVector Shove = Door->GetActorForwardVector() * (Push * (4.f + 3.f * FMath::Sin(HoldTimer * 31.f)));
		SetActorLocation(FMath::Lerp(HoldFromLocation, Door->GetHoldStandLocation(GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), MoveT) + Shove * MoveT);
		HoldGrip = Door->GetHandleGripLocation();
		HoldPalm = Door->GetDoorPalmLocation();
		// Kol hangi yandaysa o el kolu tutar, öbür el kapıyı iter
		bHoldGripRight = FVector::DotProduct(HoldGrip - GetActorLocation(), GetActorRightVector()) > 0.f;
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
			// Yüz kapıya dönük (gövdeyle aynı yön), kolun hizasından kapıya bakar.
			// Eğim: kolun biraz üstüne -> koldaki el kadrajın altında, kapı önde. Yön kola doğru sadece biraz kayar.
			const FVector Eye = FirstPersonCamera->GetComponentLocation();
			const FRotator ToGrip = (HoldGrip + FVector(0.f, 0.f, HoldLookAboveGrip) - Eye).Rotation();
			const float DoorYaw = (-Door->GetActorForwardVector()).Rotation().Yaw;
			FRotator Look(ToGrip.Pitch, DoorYaw + 0.3f * FMath::FindDeltaAngleDegrees(DoorYaw, ToGrip.Yaw), 0.f);
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
	In.ReachTargetL = HoldReach > Reach ? (bHoldGripRight ? HoldPalm : HoldGrip) : PickupCanTop;
	In.bHoldFlashlight = !Flashlight->IsDocked();
	In.FlashlightAim = FlashAim;
	In.WorkAlpha = WorkAlpha;
	In.ReachAlphaR = FMath::Max(ReachR, HoldReach);
	In.ReachTargetR = HoldReach > ReachR ? (bHoldGripRight ? HoldGrip : HoldPalm) : ReachTargetR;
	In.DeskHandsAlpha = DeskHands;
	// Kapıya yüklenirken gövde öne eğik (zorlama anında daha çok)
	In.ExtraLean = ExtraLean + HoldReach * (HoldLean + 4.f * (Door ? Door->GetForceAlpha() : 0.f));
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

void ANLPlayerCharacter::SetCanSource(ANLFuelCan* Can)
{
	CanSource = Can;
}

void ANLPlayerCharacter::DropFuelCan()
{
	if (!bHasFuelCan || Mode != ENLPlayerMode::Walking || IsBusy() || !CanSource.IsValid()) return;
	// Önündeki zemin (duvara/eşyaya gömülmesin diye önce önü boş mu bak)
	const FVector Fwd = FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLDropCan), false, this);
	FHitResult Hit;
	float Dist = 60.f;
	const FVector Eye = GetActorLocation();
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Fwd * 90.f, ECC_Visibility, Q)) Dist = FMath::Max(15.f, Hit.Distance - 30.f);
	const FVector P = Eye + Fwd * Dist;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, P, P - FVector(0.f, 0.f, 250.f), ECC_Visibility, Q)) return;
	DropSpot = Hit.ImpactPoint;
	bDroppingCan = true;
	bDropDone = false;
	PickupTimer = 0.f;
	PickupCanTop = DropSpot + FVector(0.f, 0.f, 50.f);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	const FVector ToSpot = PickupCanTop - FirstPersonCamera->GetComponentLocation();
	PickupLookRotation = FRotator(FMath::Clamp(ToSpot.Rotation().Pitch, -70.f, -20.f), ToSpot.Rotation().Yaw, 0.f);
	UE_LOG(LogTemp, Log, TEXT("NLPlayer: dropping fuel can"));
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
			// Kutunun önünde (yerel +Y), zemine oturt, kabine bak
			const FVector C = F->GetActorTransform().TransformPosition(FVector(0.f, 10.f, 34.f));
			FVector P = F->GetActorTransform().TransformPosition(FVector(-20.f, 150.f, 0.f));
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NLFuseShot), false, this);
			if (GetWorld()->LineTraceSingleByChannel(Hit, P, P - FVector(0, 0, 400.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f;
			SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
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
			if (F->GetFuseInSlot(j) == F->GetTargetFuse(j)) continue;
			int32 Want = j;
			for (int32 k = 0; k < ANLFuseBox::NumFuses; ++k) { if (F->GetTargetFuse(k) == F->GetFuseInSlot(j)) { Want = k; break; } }
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
			if (F->GetFuseInSlot(i) == F->GetTargetFuse(i)) continue;
			for (int32 j = 0; j < ANLFuseBox::NumFuses; ++j)
			{
				if (F->GetFuseInSlot(j) == F->GetTargetFuse(i) && FuseWidget->TryPickFuse(j)) { FuseWidget->DropFuse(i); break; }
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

void ANLPlayerCharacter::NLDebugAda()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const FString& Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s loc=(%.0f,%.0f,%.0f) mode=%d"), *Name, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, int32(Mode));
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	ANLSecurityDesk* Desk = nullptr;
	for (TActorIterator<ANLSecurityDesk> It(GetWorld()); It; ++It) { Desk = *It; break; }

	At(1.5f, [Shot]() { Shot(TEXT("201 start")); });
	At(2.f, [this, Desk]()
	{
		if (!Desk) return;
		const FTransform Seat = Desk->GetSeatTransform();
		SetActorLocation(Seat.GetLocation() - Seat.GetRotation().GetForwardVector() * 60.f + FVector(0, 0, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
		SitAtDesk(Desk);
	});
	At(4.f, [Shot]() { Shot(TEXT("202 seated")); });
	for (int32 i = 0; i < 8; ++i)
	{
		At(4.5f + i * 1.6f, [Desk, i]() { if (Desk) Desk->SwitchToCamera(i); });
		At(5.7f + i * 1.6f, [Shot, Desk, i]()
		{
			const ANLSecurityCamera* Cam = Desk ? Desk->GetActiveCamera() : nullptr;
			Shot(FString::Printf(TEXT("21%d cam %s"), i, Cam ? *Cam->GetLocationLabel().ToString() : TEXT("?")));
		});
	}
	At(18.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugChase(float X, float Y)
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	ANLRunner* Runner = nullptr;
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) { Runner = *It; break; }

	At(0.5f, [this, X, Y]()
	{
		FHitResult Hit;
		const FVector Top(X, Y, 3000.f);
		const float Z = GetWorld()->LineTraceSingleByChannel(Hit, Top, FVector(X, Y, -3000.f), ECC_Visibility) ? Hit.ImpactPoint.Z : GetActorLocation().Z;
		SetActorLocation(FVector(X, Y, Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	At(8.f, [this, Runner]()
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			const ANavigationData* ND = Nav->GetDefaultNavDataInstance();
			FNavLocation PP, RP;
			const bool bP = Nav->ProjectPointToNavigation(GetActorLocation(), PP, FVector(200, 200, 400));
			const bool bR = Runner && Nav->ProjectPointToNavigation(Runner->GetActorLocation(), RP, FVector(200, 200, 400));
			UE_LOG(LogTemp, Log, TEXT("NLDebugChase: nav=%s building=%d bounds=%d navBounds=%s playerOnNav=%d runnerOnNav=%d"),
				*GetNameSafe(ND), Nav->IsNavigationBuildInProgress() ? 1 : 0, Nav->GetNavigationBounds().Num(),
				ND ? *ND->GetBounds().ToString() : TEXT("-"), bP ? 1 : 0, bR ? 1 : 0);
		}
		if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>()) GM->RequestRunnerChase(this);
	});
	for (int32 s = 0; s < 22; ++s)
	{
		At(9.f + s, [this, Runner, s]()
		{
			if (!Runner) return;
			const float D = FVector::Dist2D(Runner->GetActorLocation(), GetActorLocation());
			UE_LOG(LogTemp, Log, TEXT("NLDebugChase: t=%d runner=(%.0f,%.0f,%.0f) dist=%.0f chasing=%d player_mode=%d"), s, Runner->GetActorLocation().X, Runner->GetActorLocation().Y, Runner->GetActorLocation().Z, D, Runner->IsChasing() ? 1 : 0, int32(Mode));
			if (s == 3 || s == 10)
			{
				if (Controller) Controller->SetControlRotation((Runner->GetActorLocation() - GetActorLocation()).Rotation());
				if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
			}
		});
	}
	At(32.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugAmbush()
{
	ANLRunnerAmbushZone* Zone = nullptr;
	for (TActorIterator<ANLRunnerAmbushZone> It(GetWorld()); It; ++It) { Zone = *It; break; }
	ANLHideDoor* Door = nullptr;
	for (TActorIterator<ANLHideDoor> It(GetWorld()); It; ++It) { Door = *It; break; }
	ANLRunner* Runner = nullptr;
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) { Runner = *It; break; }
	if (!Zone || !Door || !Runner) { UE_LOG(LogTemp, Warning, TEXT("NLDebugAmbush: zone/door/runner missing")); return; }

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
	auto PlaceOnGround = [this](FVector P)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("NLAmb"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 400.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		SetActorLocation(P + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	};
	// Kapının dışına doğru yön
	const FVector Out = (Door->GetOutsideLocation() - Door->GetActorLocation()).GetSafeNormal2D();
	At(0.5f, [=, this]() { PlaceOnGround(Door->GetActorLocation() + Out * (Zone->Radius + 600.f)); });   // önce alanın dışında
	At(3.f, [=, this]() { Zone->SetDebugForce(true); PlaceOnGround(Door->GetActorLocation() + Out * 350.f); if (Controller) Controller->SetControlRotation((-Out).Rotation()); });
	At(3.6f, [Shot]() { Shot(TEXT("301 near the shack, scream")); });

	TSharedRef<int32> State = MakeShared<int32>(0);
	FTimerHandle Poll;
	TM.SetTimer(Poll, FTimerDelegate::CreateWeakLambda(this, [this, Door, Runner, State, Shot]()
	{
		if (*State == 0 && Runner->IsChasing() && !Runner->IsHidden())
		{
			const float D = FVector::Dist2D(Runner->GetActorLocation(), GetActorLocation());
			UE_LOG(LogTemp, Log, TEXT("NLDebugAmbush: runner running, dist=%.0f"), D);
			if (D < 900.f)
			{
				*State = 1;
				if (Controller) Controller->SetControlRotation((Runner->GetActorLocation() - GetActorLocation()).Rotation());
				Shot(TEXT("302 runner coming"));
				FTimerHandle H;
				GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, Door]()
				{
					// Kulübeye gir ve kapıyı kapat
					SetActorLocation(Door->GetHoldStandLocation(GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
					if (Door->IsOpen()) Door->Interact(this);
					UE_LOG(LogTemp, Log, TEXT("NLDebugAmbush: hid in the shack, closing the door"));
				}), 0.3f, false);
			}
		}
		if (*State == 1 && Mode == ENLPlayerMode::HoldingDoor)
		{
			*State = 2;
			// Duruş: birinci şahıs, yandan ve arkadan
			FTimerManager& T = GetWorldTimerManager();
			FTimerHandle A, B, C, D, E, F;
			T.SetTimer(A, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("311 hold: first person")); }), 1.6f, false);
			T.SetTimer(B, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(true, FVector(20.f, -110.f, 40.f)); }), 1.8f, false);
			T.SetTimer(C, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("312 hold: side view")); }), 2.0f, false);
			T.SetTimer(D, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(true, FVector(-150.f, -40.f, 90.f)); }), 2.2f, false);
			T.SetTimer(E, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("313 hold: behind")); }), 2.4f, false);
			T.SetTimer(F, FTimerDelegate::CreateWeakLambda(this, [this]() { DebugThirdPerson(false); }), 2.6f, false);
		}
		if (*State == 2)
		{
			Door->SetDebugAutoResult(1);
			if (Mode == ENLPlayerMode::Walking)
			{
				*State = 3;
				UE_LOG(LogTemp, Log, TEXT("NLDebugAmbush: survived"));
				Shot(TEXT("303 survived, runner gone"));
				FTimerHandle Q;
				GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 1.5f, false);
			}
		}
		if (Mode == ENLPlayerMode::Dead && *State < 3)
		{
			*State = 3;
			UE_LOG(LogTemp, Log, TEXT("NLDebugAmbush: player died"));
			FTimerHandle Q;
			GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 1.f, false);
		}
	}), 0.1f, true);
	At(70.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugRefuel()
{
	ANLGenerator* Gen = nullptr;
	for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It) { Gen = *It; break; }
	ANLFuelCan* Can = nullptr;
	for (TActorIterator<ANLFuelCan> It(GetWorld()); It; ++It) { Can = *It; break; }
	if (!Gen || !Can) { UE_LOG(LogTemp, Warning, TEXT("NLDebugRefuel: generator/can missing")); return; }

	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const FString& Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugShots: %s"), *Name);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	auto StandAt = [this](FVector P, const FVector& LookAt)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("NLRef"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 300.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		SetActorLocation(P + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation((LookAt - FirstPersonCamera->GetComponentLocation()).Rotation());
	};

	// Bidonu al
	At(0.5f, [this, Can, StandAt]() { StandAt(Can->GetActorLocation() + FVector(80.f, 0, 0), Can->GetActorLocation()); });
	At(1.2f, [this, Can, Gen]() { Can->Interact(this); UE_LOG(LogTemp, Log, TEXT("NLDebugRefuel: picked can, generator fuel=%.1f"), Gen->GetFuelPercent()); });
	// Benzin ağzının önüne git (binanın güneyinde)
	At(3.5f, [this, Gen, StandAt]()
	{
		const FVector Inlet = Gen->GetPourPoint(Gen->GetActorLocation() - Gen->GetActorRightVector() * 500.f);
		const FVector Front = Inlet - Gen->GetActorRightVector() * 90.f;
		StandAt(Front, Inlet);
	});
	At(4.3f, [Shot, Gen, this]() { Shot(FString::Printf(TEXT("501 at the fuel inlet, prompt: %s"), *Gen->GetInteractPrompt(this).ToString())); });
	At(4.6f, [this, Gen]() { Gen->Interact(this); });
	At(5.8f, [Shot]() { Shot(TEXT("502 pouring fuel")); });
	At(8.5f, [this, Gen]() { UE_LOG(LogTemp, Log, TEXT("NLDebugRefuel: after pour fuel=%.1f producing=%d"), Gen->GetFuelPercent(), Gen->IsProducingPower() ? 1 : 0); });
	// Arıza ve tamir
	At(9.f, [this, Gen]() { Gen->BreakDown(); Gen->Interact(this); UE_LOG(LogTemp, Log, TEXT("NLDebugRefuel: broke down, repair mode=%d"), int32(Mode)); });
	At(10.f, [Shot]() { Shot(TEXT("503 repairing the generator")); });
	At(10.5f, [this, Gen]() { Gen->CancelRepair(); });
	At(11.5f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugDrownedAway()
{
	ANLDrownedDirector* Dir = nullptr;
	for (TActorIterator<ANLDrownedDirector> It(GetWorld()); It; ++It) { Dir = *It; break; }
	if (!Dir) return;
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	// Oyuncu dışarıda (kavşak), Drowned hemen gelmeye başlar; oyuncu hiç ışık tutmaz
	At(0.5f, [this]()
	{
		FVector P(54.f, -500.f, 0.f);
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("NLAway"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 3000.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		SetActorLocation(P + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	At(1.5f, [Dir]() { Dir->DebugAppear(-1); });
	for (int32 s = 10; s <= 120; s += 10)
	{
		At(1.5f + s, [this, s]() { UE_LOG(LogTemp, Log, TEXT("NLDebugDrownedAway: t=%d s player_mode=%d (5 = dead)"), s, int32(Mode)); });
	}
	At(124.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugChaos()
{
	// Kaos kuralı testi: Drowned gelirken koşucu kovalar -> çekilmeli; kovalama sürerken belirmemeli;
	// sakinleşince gelmeli; jeneratör bozulunca yine çekilmeli
	ANLDrownedDirector* Dir = nullptr;
	for (TActorIterator<ANLDrownedDirector> It(GetWorld()); It; ++It) { Dir = *It; break; }
	ANLRunner* Runner = nullptr;
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) { Runner = *It; break; }
	ANLGenerator* Gen = nullptr;
	for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It) { Gen = *It; break; }
	if (!Dir || !Runner || !Gen) { UE_LOG(LogTemp, Warning, TEXT("NLDebugChaos: missing actors")); return; }
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	At(1.f, [Dir]() { UE_LOG(LogTemp, Log, TEXT("NLDebugChaos: drowned appears")); Dir->DebugAppear(0); });
	At(4.f, [this, Runner]()
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugChaos: runner chase starts"));
		Runner->StartChase(this, FTransform(GetActorLocation() + FVector(4000.f, 0.f, 0.f)));
	});
	At(28.f, [Runner]() { UE_LOG(LogTemp, Log, TEXT("NLDebugChaos: runner chase stops")); Runner->StopChase(); });
	At(70.f, [Gen]() { UE_LOG(LogTemp, Log, TEXT("NLDebugChaos: generator breaks")); Gen->BreakDown(); });
	for (int32 s = 2; s <= 90; s += 2)
	{
		At(float(s), [this, Dir, s]()
		{
			UE_LOG(LogTemp, Log, TEXT("NLDebugChaos: t=%d drowned_state=%d mode=%d"), s, int32(Dir->GetState()), int32(Mode));
		});
	}
	At(92.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugDiag()
{
	// Teşhis sadece masada + fener pili dışarıda hızlı biter
	ANLFuseBox* Fuse = nullptr;
	for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It) { Fuse = *It; break; }
	ANLGenerator* Gen = nullptr;
	for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It) { Gen = *It; break; }
	ANLSecurityDesk* Desk = nullptr;
	for (TActorIterator<ANLSecurityDesk> It(GetWorld()); It; ++It) { Desk = *It; break; }
	if (!Fuse || !Gen || !Desk) { UE_LOG(LogTemp, Warning, TEXT("NLDebugDiag: missing actors")); return; }
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Ground = [this](FVector P)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("NLDiag"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 3000.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		SetActorLocation(P + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	};
	auto Batt = [this](const TCHAR* Tag)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDebugDiag: %s battery=%.1f%% outdoors=%d on=%d"), Tag, Flashlight->GetBatteryPercent() * 100.f, Flashlight->IsOutdoors() ? 1 : 0, Flashlight->IsOn() ? 1 : 0);
	};
	// 1) Dışarıda (kavşak), fener açık; elektrik kesilir -> sadece "monitöre bak" uyarısı
	At(1.0f, [this, Ground]() { Ground(FVector(54.f, -500.f, 0.f)); if (!Flashlight->IsOn()) Flashlight->Toggle(); });
	At(2.0f, [Batt]() { Batt(TEXT("outside t0")); });
	At(2.2f, [Fuse]() { Fuse->SabotageWith(true); });
	At(3.0f, [this]() { { UE_LOG(LogTemp, Log, TEXT("NLDebugShots: diag 1 outside: power out warning")); if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui")); } });
	At(12.0f, [Batt]() { Batt(TEXT("outside t10")); });
	// 2) İçeride (kontrol odası, masanın arkası), fener açık
	At(12.5f, [this, Desk]()
	{
		const FTransform Seat = Desk->GetSeatTransform();
		SetActorLocation(Seat.GetLocation() - Seat.GetRotation().GetForwardVector() * 90.f, false, nullptr, ETeleportType::TeleportPhysics);
	});
	At(13.5f, [Batt]() { Batt(TEXT("inside t0")); });
	At(23.5f, [Batt]() { Batt(TEXT("inside t10")); });
	// 3) Masaya otur: teşhis paneli (elektrik yok, sigorta kablolaması yanlış)
	At(24.0f, [this, Desk]()
	{
		const FTransform Seat = Desk->GetSeatTransform();
		if (Controller) Controller->SetControlRotation(FRotator(0.f, Seat.Rotator().Yaw, 0.f));
		SitAtDesk(Desk);
	});
	At(28.0f, [this]() { { UE_LOG(LogTemp, Log, TEXT("NLDebugShots: diag 2 desk: fuse wiring mismatch, no power")); if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui")); } });
	// Yanlış diziliyken şalteri kaldır -> geri düşmeli
	At(28.5f, [Fuse]() { Fuse->SetOn(true); });
	At(29.5f, [Fuse]() { UE_LOG(LogTemp, Log, TEXT("NLDebugDiag: after wrong lever switch=%s power_ok=%d"), Fuse->IsOn() ? TEXT("ON") : TEXT("OFF"), Fuse->IsPowerOk() ? 1 : 0); });
	// Monitördeki sıraya göre düzelt, şalteri aç
	At(30.0f, [Fuse]()
	{
		for (int32 i = 0; i < ANLFuseBox::NumFuses; ++i)
		{
			for (int32 j = i; j < ANLFuseBox::NumFuses; ++j)
			{
				if (Fuse->GetFuseInSlot(j) == Fuse->GetTargetFuse(i)) { Fuse->SwapFuses(i, j); break; }
			}
		}
		Fuse->SetOn(true);
		UE_LOG(LogTemp, Log, TEXT("NLDebugDiag: fixed by monitor order, power_ok=%d"), Fuse->IsPowerOk() ? 1 : 0);
	});
	At(32.0f, [this]() { { UE_LOG(LogTemp, Log, TEXT("NLDebugShots: diag 3 desk: power ok")); if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui")); } });
	At(32.5f, [Gen]() { Gen->BreakDown(); });
	At(34.0f, [this]() { { UE_LOG(LogTemp, Log, TEXT("NLDebugShots: diag 4 desk: generator fault")); if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui")); } });
	At(35.0f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugDrownedRoutes()
{
	ANLDrownedDirector* Dir = nullptr;
	for (TActorIterator<ANLDrownedDirector> It(GetWorld()); It; ++It) { Dir = *It; break; }
	ANLSecurityDesk* Desk = nullptr;
	for (TActorIterator<ANLSecurityDesk> It(GetWorld()); It; ++It) { Desk = *It; break; }
	if (!Dir || !Desk) return;
	for (int32 i = 0; i < Desk->GetCameras().Num(); ++i)
	{
		Dir->DebugAppear(i);
		const ANLDrowned* D = Dir->GetDrowned();
		UE_LOG(LogTemp, Log, TEXT("NLDebugRoutes: cam %d %s -> %s"), i + 1, *Desk->GetCameras()[i]->GetLocationLabel().ToString(), D && D->IsRunning() ? TEXT("runs in") : TEXT("TELEPORTS (no route)"));
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
}

void ANLPlayerCharacter::NLDebugDrowned(int32 CamIndex)
{
	ANLDrownedDirector* Dir = nullptr;
	for (TActorIterator<ANLDrownedDirector> It(GetWorld()); It; ++It) { Dir = *It; break; }
	ANLSecurityDesk* Desk = nullptr;
	for (TActorIterator<ANLSecurityDesk> It(GetWorld()); It; ++It) { Desk = *It; break; }
	if (!Dir || !Desk || !Desk->GetCameras().IsValidIndex(CamIndex)) { UE_LOG(LogTemp, Warning, TEXT("NLDebugDrowned: director/desk/camera missing")); return; }
	const FTransform Spot = Desk->GetCameras()[CamIndex]->GetDrownedSpawnTransform();

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
	auto LookAtDrowned = [this, Dir]()
	{
		if (ANLDrowned* D = Dir->GetDrowned())
		{
			if (Controller) Controller->SetControlRotation((D->GetActorLocation() + FVector(0, 0, 30.f) - FirstPersonCamera->GetComponentLocation()).Rotation());
		}
	};

	// Oyuncu dışarıda, kameranın tarafında; Drowned'ın geleceği noktaya bakıyor
	At(0.5f, [this, Spot]()
	{
		FVector P = Spot.GetLocation() + Spot.GetRotation().GetForwardVector().GetSafeNormal2D() * 650.f;
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("NLDrw"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 600.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		SetActorLocation(P + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation((Spot.GetLocation() - P).Rotation() + FRotator(-5.f, 0, 0));
		if (Flashlight->IsOn()) Flashlight->Toggle();
	});
	At(2.5f, [Dir, CamIndex]() { Dir->DebugAppear(CamIndex); });
	At(3.3f, [LookAtDrowned, Shot]() { LookAtDrowned(); Shot(TEXT("401 drowned running in")); });
	At(4.5f, [LookAtDrowned, Shot]() { LookAtDrowned(); Shot(TEXT("402 drowned still running")); });

	TSharedRef<int32> State = MakeShared<int32>(0);
	FTimerHandle Poll;
	TM.SetTimer(Poll, FTimerDelegate::CreateWeakLambda(this, [this, Dir, State, Shot, LookAtDrowned]()
	{
		ANLDrowned* D = Dir->GetDrowned();
		if (*State == 0 && Dir->GetState() == ENLDrownedState::Watching)
		{
			*State = 1;
			LookAtDrowned();
			Shot(TEXT("403 drowned arrived, waiting"));
			// Feneri aç ve ona tut
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, LookAtDrowned]()
			{
				if (!Flashlight->IsOn()) Flashlight->Toggle();
				LookAtDrowned();
				UE_LOG(LogTemp, Log, TEXT("NLDebugDrowned: flashlight on the drowned"));
			}), 0.8f, false);
		}
		if (*State == 1 && Dir->GetState() == ENLDrownedState::Flinching)
		{
			*State = 2;
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Shot]() { Shot(TEXT("404 flashlight: drowned covers its face")); }), 0.4f, false);
		}
		if (*State >= 1 && *State < 3) LookAtDrowned();
		if (*State == 2 && D && D->IsRunning())
		{
			*State = 3;
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Shot, LookAtDrowned]() { LookAtDrowned(); Shot(TEXT("405 drowned runs back along its route")); }), 0.8f, false);
		}
		if (*State == 3 && D && !D->IsShown())
		{
			*State = 4;
			UE_LOG(LogTemp, Log, TEXT("NLDebugDrowned: drowned vanished at the end of its route"));
			FTimerHandle Q;
			GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 1.f, false);
		}
	}), 0.05f, true);
	At(45.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugAmbience()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Amb = [this]() -> ANLAmbienceDirector* { for (TActorIterator<ANLAmbienceDirector> It(GetWorld()); It; ++It) return *It; return nullptr; };
	// İçeride (başlangıç: fenerin zemin katı) birkaç ses, sonra dışarı (kavşak) ve birkaç ses daha
	At(2.f, [Amb]() { if (ANLAmbienceDirector* A = Amb()) A->DebugBurst(3, 2.f); });
	At(9.f, [this]()
	{
		FVector P(54.f, -500.f, 0.f);
		FHitResult Hit;
		FCollisionQueryParams Q(TEXT("NLAmb"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 3000.f), P - FVector(0, 0, 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		SetActorLocation(P + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
	});
	At(12.f, [Amb]() { if (ANLAmbienceDirector* A = Amb()) A->DebugBurst(5, 2.f); });
	for (int32 s = 1; s <= 22; s += 3)
	{
		At(float(s), [Amb, s]() { if (ANLAmbienceDirector* A = Amb()) UE_LOG(LogTemp, Log, TEXT("NLDebugAmbience: t=%d indoor=%.2f"), s, A->GetIndoorAlpha()); });
	}
	At(24.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugTour()
{
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *(FPaths::ProjectSavedDir() / TEXT("tour.txt")));
	struct FStop { FVector Loc; FRotator Rot; FString Name; bool bFlash; };
	TArray<FStop> Stops;
	for (const FString& L : Lines)
	{
		TArray<FString> P;
		L.TrimStartAndEnd().ParseIntoArrayWS(P);
		if (P.Num() < 5 || P[0].StartsWith(TEXT("#"))) continue;
		FStop S{ FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])), FRotator(FCString::Atof(*P[3]), FCString::Atof(*P[4]), 0.f), P.Num() > 5 ? P[5] : TEXT("stop"), P.Num() > 6 && P[6] == TEXT("F") };
		Stops.Add(S);
	}
	UE_LOG(LogTemp, Log, TEXT("NLDebugTour: %d stops"), Stops.Num());
	SetActorEnableCollision(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	FTimerManager& TM = GetWorldTimerManager();
	for (int32 i = 0; i < Stops.Num(); ++i)
	{
		const FStop S = Stops[i];
		FTimerHandle H1, H2;
		TM.SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			SetActorLocation(S.Loc, false, nullptr, ETeleportType::TeleportPhysics);
			// 7. sütun F: bu durakta el feneri açık
			if (Flashlight->IsOn() != S.bFlash) Flashlight->Toggle();
			GetCharacterMovement()->StopMovementImmediately();
			if (Controller) Controller->SetControlRotation(S.Rot);
		}), 0.5f + i * 2.5f, false);
		TM.SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			UE_LOG(LogTemp, Log, TEXT("NLDebugShots: tour %s"), *S.Name);
			// Ölçüm: aşağı ve dört yana çarpışma mesafeleri
			FCollisionQueryParams Q(TEXT("NLTour"), true, this);
			auto Probe = [&](const FVector& Dir, const TCHAR* Tag)
			{
				FHitResult Hit;
				if (GetWorld()->LineTraceSingleByChannel(Hit, S.Loc, S.Loc + Dir * 5000.f, ECC_Visibility, Q))
					UE_LOG(LogTemp, Log, TEXT("NLTourProbe: %s %s hit=(%.0f,%.0f,%.0f) actor=%s"), *S.Name, Tag, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, *GetNameSafe(Hit.GetActor()));
			};
			Probe(FVector::DownVector, TEXT("down"));
			Probe(FVector::ForwardVector, TEXT("+x"));
			Probe(-FVector::ForwardVector, TEXT("-x"));
			Probe(FVector::RightVector, TEXT("+y"));
			Probe(-FVector::RightVector, TEXT("-y"));
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
		}), 2.3f + i * 2.5f, false);
	}
	FTimerHandle HQ;
	TM.SetTimer(HQ, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
	}), 1.f + Stops.Num() * 2.5f, false);
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
		const float Target = (bSprinting ? SprintSpeed : WalkSpeed) * (bHasFuelCan ? CarryCanSpeedMultiplier : 1.f);
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

void ANLPlayerCharacter::NLDebugRainShots()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Cmd = [this](const TCHAR* C)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(C);
	};
	auto Look = [this](float X, float Y, float Pitch, float Yaw)
	{
		SetActorLocation(FVector(X, Y, GetActorLocation().Z + 50.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
	};
	At(1.6f, [=, this]() { UE_LOG(LogTemp, Log, TEXT("NLRainShots: indoors at start")); Cmd(TEXT("shot showui")); });
	At(2.f, [=, this]() { Look(54.f, -500.f, 0.f, 90.f); if (Flashlight && !Flashlight->IsOn()) Flashlight->Toggle(); });
	At(5.f, [=, this]() { UE_LOG(LogTemp, Log, TEXT("NLRainShots: outside, flashlight")); Cmd(TEXT("shot showui")); });
	At(6.f, [=, this]() { if (Flashlight && Flashlight->IsOn()) Flashlight->Toggle(); });
	At(8.f, [=, this]() { UE_LOG(LogTemp, Log, TEXT("NLRainShots: outside, no flashlight")); Cmd(TEXT("shot showui")); });
	At(9.f, [=, this]() { Cmd(TEXT("NLDebugLightning")); });
	At(9.06f, [=, this]() { UE_LOG(LogTemp, Log, TEXT("NLRainShots: lightning")); Cmd(TEXT("shot showui")); });
	At(14.f, [=, this]() { Look(54.f, -500.f, 0.f, -90.f); });
	At(16.f, [=, this]() { UE_LOG(LogTemp, Log, TEXT("NLRainShots: other way")); Cmd(TEXT("shot showui")); });
	At(17.f, [=, this]() { Cmd(TEXT("NLDebugEndDayIn 0")); });
	At(32.f, [=, this]() { Look(54.f, -500.f, 0.f, 90.f); });
	At(34.f, [=, this]() { UE_LOG(LogTemp, Log, TEXT("NLRainShots: night 2, no rain")); Cmd(TEXT("shot showui")); });
	At(36.f, [=, this]() { Cmd(TEXT("quit")); });
}

void ANLPlayerCharacter::SetCinematicMode(bool bOn)
{
	if (bCinematic == bOn) return;
	bCinematic = bOn;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (HUDWidget) HUDWidget->SetVisibility(bOn ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (PC)
	{
		if (bOn) DisableInput(PC); else EnableInput(PC);
		if (!bOn) RestoreGameInput();
		PC->SetIgnoreLookInput(bOn);
		PC->SetIgnoreMoveInput(bOn);
	}
	UE_LOG(LogTemp, Log, TEXT("NLPlayer: cinematic mode %s"), bOn ? TEXT("ON") : TEXT("OFF"));
}

void ANLPlayerCharacter::NLDebugIntroShots()
{
	FTimerManager& TM = GetWorldTimerManager();
	const float Times[] = { 2.5f, 5.f, 8.f, 10.5f, 13.f, 15.5f, 17.f, 18.6f, 19.3f, 20.2f, 21.8f, 22.6f, 24.f, 26.f, 30.f, 36.f };
	for (float T : Times)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, T]()
		{
			UE_LOG(LogTemp, Log, TEXT("NLIntroShots: t=%.1f"), T);
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
		}), T, false);
	}
	FTimerHandle Q;
	TM.SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
	}), 38.f, false);
}

void ANLPlayerCharacter::NLDebugArtTour()
{
	struct FStop { const TCHAR* Name; float X, Y, Pitch, Yaw; bool bLight; };
	static const FStop Stops[] = {
		{ TEXT("dock cabin front"),   -2819.f, 3195.f, 4.f, -127.f, true },
		{ TEXT("dock cabin no light"),-2819.f, 3195.f, 4.f, -127.f, false },
		{ TEXT("dock toward sea"),    -2850.f, 2950.f, 0.f, 150.f, true },
		{ TEXT("ruined cabin"),        2350.f, 1560.f, 4.f, -70.f, true },
		{ TEXT("forest"),              1582.f, -2500.f, 2.f, -90.f, true },
		{ TEXT("lighthouse path"),       54.f, -500.f, 6.f, -60.f, true },
		{ TEXT("generator"),            300.f, -200.f, 0.f, -141.f, true },
		{ TEXT("ground close"),          54.f, -500.f, -55.f, -60.f, true },
		{ TEXT("shore rocks"),        -2600.f, -3400.f, 0.f, -135.f, true },
		{ TEXT("overview lighthouse"),   925.f, -2300.f, -12.f, 125.f, false },
	};
	FTimerManager& TM = GetWorldTimerManager();
	const float Step = 5.f;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Stops); ++i)
	{
		const FStop S = Stops[i];
		const float T0 = 1.f + i * Step;
		FTimerHandle H1, H2, H3;
		TM.SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			// Ağacın içine düşmesin: yakınında çam olmayan ilk noktayı bul
			FVector2D P(S.X, S.Y);
			TArray<UInstancedStaticMeshComponent*> Trees;
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (It->GetClass()->GetName() != TEXT("NLEnvScatter")) continue;
				TArray<UInstancedStaticMeshComponent*> C;
				It->GetComponents(C);
				for (UInstancedStaticMeshComponent* I : C) if (I->GetName().Contains(TEXT("Pine"))) Trees.Add(I);
			}
			auto Clear = [&](const FVector2D& Q2)
			{
				for (UInstancedStaticMeshComponent* I : Trees) if (I->GetInstancesOverlappingSphere(FVector(Q2, 300.f), 420.f, true).Num() > 0) return false;
				return true;
			};
			if (!Clear(P))
			{
				for (float R = 250.f; R <= 1250.f; R += 250.f)
				{
					bool bFound = false;
					for (int32 k = 0; k < 8 && !bFound; ++k)
					{
						const FVector2D C2 = P + FVector2D(FMath::Cos(k * PI / 4), FMath::Sin(k * PI / 4)) * R;
						if (Clear(C2)) { P = C2; bFound = true; }
					}
					if (bFound) break;
				}
			}
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NLTour), false, this);
			float Z = GetActorLocation().Z;
			const float Top = FCString::Strstr(S.Name, TEXT("overview")) ? 1000.f : ((FCString::Strstr(S.Name, TEXT("inside")) || FCString::Strstr(S.Name, TEXT("room"))) ? 640.f : 5000.f);
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(P, Top), FVector(P, -2000.f), ECC_Visibility, Q)) Z = Hit.ImpactPoint.Z + 100.f;
			SetActorLocation(FVector(P, Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation(FRotator(S.Pitch, S.Yaw, 0.f));
			if (Flashlight && Flashlight->IsOn() != S.bLight) Flashlight->Toggle();
		}), T0, false);
		// Kare süresi: 1.5 - 4.0 sn arası
		TM.SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TourFrame0 = GFrameCounter;
			TourTime0 = FPlatformTime::Seconds();
		}), T0 + 1.5f, false);
		TM.SetTimer(H3, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			const double Ms = (FPlatformTime::Seconds() - TourTime0) * 1000.0 / FMath::Max<uint64>(1, GFrameCounter - TourFrame0);
			UE_LOG(LogTemp, Log, TEXT("NLArtTour: %s  %.1f ms/frame (%.0f fps)"), S.Name, Ms, 1000.0 / FMath::Max(Ms, 0.01));
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
		}), T0 + 4.f, false);
	}
	FTimerHandle Q;
	TM.SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
	}), 1.f + UE_ARRAY_COUNT(Stops) * Step + 1.f, false);
}

void ANLPlayerCharacter::NLDebugLighthouseTour()
{
	struct FStop { const TCHAR* Name; float X, Y, Pitch, Yaw; bool bLight; float TopZ; };
	static const FStop Stops[] = {
		{ TEXT("keeper exterior"),        300.f, -1100.f, 12.f, -75.f, false, 5000.f },
		{ TEXT("keeper to tower"),        200.f, -1950.f, 0.f, -5.f, true, 640.f },
		{ TEXT("desk chair crt"),         312.f, -1880.f, -18.f, -90.f, true, 640.f },
		{ TEXT("fuel can dock shack"),  -3230.f, 2600.f, -35.f, 35.f, true, 300.f },
		{ TEXT("fuel area cans"),       -1700.f, 2700.f, -20.f, 136.f, true, 5000.f },
		{ TEXT("lighthouse far"),        2900.f, 200.f, 10.f, -125.f, false, 5000.f },
	};
	FTimerManager& TM = GetWorldTimerManager();
	const float Step = 5.f;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Stops); ++i)
	{
		const FStop S = Stops[i];
		const float T0 = 1.f + i * Step;
		FTimerHandle H1, H2, H3;
		TM.SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			// Ağacın içine düşmesin: yakınında çam olmayan ilk noktayı bul
			FVector2D P(S.X, S.Y);
			TArray<UInstancedStaticMeshComponent*> Trees;
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (It->GetClass()->GetName() != TEXT("NLEnvScatter")) continue;
				TArray<UInstancedStaticMeshComponent*> C;
				It->GetComponents(C);
				for (UInstancedStaticMeshComponent* I : C) if (I->GetName().Contains(TEXT("Pine"))) Trees.Add(I);
			}
			auto Clear = [&](const FVector2D& Q2)
			{
				for (UInstancedStaticMeshComponent* I : Trees) if (I->GetInstancesOverlappingSphere(FVector(Q2, 300.f), 420.f, true).Num() > 0) return false;
				return true;
			};
			if (S.TopZ > 1000.f && !Clear(P))
			{
				for (float R = 250.f; R <= 1250.f; R += 250.f)
				{
					bool bFound = false;
					for (int32 k = 0; k < 8 && !bFound; ++k)
					{
						const FVector2D C2 = P + FVector2D(FMath::Cos(k * PI / 4), FMath::Sin(k * PI / 4)) * R;
						if (Clear(C2)) { P = C2; bFound = true; }
					}
					if (bFound) break;
				}
			}
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NLTour), false, this);
			float Z = GetActorLocation().Z;
			const float Top = S.TopZ;
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(P, Top), FVector(P, -2000.f), ECC_Visibility, Q)) Z = Hit.ImpactPoint.Z + 100.f;
			SetActorLocation(FVector(P, Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation(FRotator(S.Pitch, S.Yaw, 0.f));
			if (Flashlight && Flashlight->IsOn() != S.bLight) Flashlight->Toggle();
		}), T0, false);
		// Kare süresi: 1.5 - 4.0 sn arası
		TM.SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TourFrame0 = GFrameCounter;
			TourTime0 = FPlatformTime::Seconds();
		}), T0 + 1.5f, false);
		TM.SetTimer(H3, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			const double Ms = (FPlatformTime::Seconds() - TourTime0) * 1000.0 / FMath::Max<uint64>(1, GFrameCounter - TourFrame0);
			UE_LOG(LogTemp, Log, TEXT("NLLHTour: %s  %.1f ms/frame (%.0f fps)"), S.Name, Ms, 1000.0 / FMath::Max(Ms, 0.01));
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
		}), T0 + 4.f, false);
	}
	FTimerHandle Q;
	TM.SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
	}), 1.f + UE_ARRAY_COUNT(Stops) * Step + 1.f, false);
}

void ANLPlayerCharacter::NLDebugDropCan()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDropTest: %s (has can %d, speed %.0f)"), Name, bHasFuelCan ? 1 : 0, GetCharacterMovement()->MaxWalkSpeed);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};
	ANLFuelCan* Can = nullptr;
	for (TActorIterator<ANLFuelCan> It(GetWorld()); It; ++It) { Can = *It; break; }
	if (!Can) return;
	const FVector C = Can->GetActorLocation();
	At(1.f, [this, C]()
	{
		SetActorLocation(C + FVector(-90.f, 0.f, 100.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation(FRotator(-40.f, 0.f, 0.f));
		if (Flashlight && !Flashlight->IsOn()) Flashlight->Toggle();
	});
	At(2.5f, [this, Can]() { Can->Interact(this); });
	At(5.f, [this, Shot]() { Shot(TEXT("carrying")); });
	// Yürürken hız (taşırken yavaş)
	At(5.2f, [this]() { AddMovementInput(GetActorForwardVector(), 1.f); });
	At(6.f, [this]() { UE_LOG(LogTemp, Log, TEXT("NLDropTest: carry speed target %.0f (walk %.0f)"), WalkSpeed * CarryCanSpeedMultiplier, WalkSpeed); });
	At(6.5f, [this]() { DropFuelCan(); });
	At(7.2f, [this, Shot]() { Shot(TEXT("dropping")); });
	At(8.5f, [this, Shot]() { Shot(TEXT("dropped")); });
	At(9.5f, [this]()
	{
		for (TActorIterator<ANLFuelCan> It(GetWorld()); It; ++It)
		{
			if (It->CanInteract(this)) { UE_LOG(LogTemp, Log, TEXT("NLDropTest: picking up again at (%.0f, %.0f, %.0f)"), It->GetActorLocation().X, It->GetActorLocation().Y, It->GetActorLocation().Z); It->Interact(this); break; }
		}
	});
	At(12.f, [this, Shot]() { Shot(TEXT("picked up again")); });
	At(13.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::TogglePause()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
		UGameplayStatics::SetGamePaused(this, false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		if (UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this)) Sub->SaveSettings();
		return;
	}
	// Sigorta kutusu açıksa ESC önce onu kapatır; ölüyken ve sinematikte menü açılmaz
	if (Mode == ENLPlayerMode::UsingFuseBox) { CloseFuseBox(); return; }
	if (Mode == ENLPlayerMode::Dead || bCinematic) return;
	PauseMenu = CreateWidget<UNLPauseMenuWidget>(PC, UNLPauseMenuWidget::StaticClass());
	if (!PauseMenu) return;
	PauseMenu->OnResume = [this]() { TogglePause(); };
	PauseMenu->AddToViewport(50);
	UGameplayStatics::SetGamePaused(this, true);
	FInputModeUIOnly UIMode;
	UIMode.SetWidgetToFocus(PauseMenu->TakeWidget());
	UIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(UIMode);
	PC->SetShowMouseCursor(true);
}

void ANLPlayerCharacter::NLDebugPauseMenu()
{
	// Oyun duraklayınca dünya zamanlayıcıları durur: gerçek zamanlı sayaç
	TogglePause();
	TWeakObjectPtr<ANLPlayerCharacter> Self(this);
	TSharedRef<float> T = MakeShared<float>(0.f);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self, T](float Dt)
	{
		ANLPlayerCharacter* P = Self.Get();
		if (!P) return false;
		const float Prev = *T;
		*T += Dt;
		APlayerController* PC = Cast<APlayerController>(P->GetController());
		if (Prev < 1.5f && *T >= 1.5f)
		{
			UE_LOG(LogTemp, Log, TEXT("NLPauseTest: paused=%d menu=%d"), UGameplayStatics::IsGamePaused(P) ? 1 : 0, P->IsPaused() ? 1 : 0);
			if (PC) PC->ConsoleCommand(TEXT("shot showui"));
		}
		if (Prev < 3.f && *T >= 3.f)
		{
			P->TogglePause();
			UE_LOG(LogTemp, Log, TEXT("NLPauseTest: resumed paused=%d"), UGameplayStatics::IsGamePaused(P) ? 1 : 0);
		}
		if (*T >= 5.f) { if (PC) PC->ConsoleCommand(TEXT("quit")); return false; }
		return true;
	}));
}

void ANLPlayerCharacter::RestoreGameInput()
{
	// Ana menü fare imleci + arayüz modunda bırakıyor: oyunda fare doğrudan bakışa gitsin (sağ tık gerekmesin)
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->bEnableClickEvents = false;
		PC->bEnableMouseOverEvents = false;
		UE_LOG(LogTemp, Log, TEXT("NLPlayer: game input mode (cursor hidden)"));
	}
}

void ANLPlayerCharacter::NLDebugHandShots()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const TCHAR* Name)
	{
		UE_LOG(LogTemp, Log, TEXT("NLHandTest: %s"), Name);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot"));
	};
	// Sürekli ileri yürü
	static FTimerHandle Walk;
	TM.SetTimer(Walk, FTimerDelegate::CreateWeakLambda(this, [this]() { AddMovementInput(GetActorForwardVector(), 1.f); }), 0.016f, true, 1.0f);
	At(0.5f, [this]() { if (Controller) Controller->SetControlRotation(FRotator(-10.f, GetActorRotation().Yaw, 0.f)); if (!Flashlight->IsOn()) Flashlight->Toggle(); });
	const FVector Side(45.f, 150.f, 40.f);
	At(2.0f, [this, Side, Shot]() { DebugThirdPerson(true, Side); Shot(TEXT("walk side A")); });
	At(2.27f, [this, Side, Shot]() { DebugThirdPerson(true, Side); Shot(TEXT("walk side B")); });
	At(2.55f, [this, Side, Shot]() { DebugThirdPerson(true, Side); Shot(TEXT("walk side C")); });
	At(2.9f, [this, Shot]() { DebugThirdPerson(true, FVector(70.f, 40.f, 20.f)); Shot(TEXT("walk front")); });
	At(3.3f, [this, Shot]() { DebugThirdPerson(false); Shot(TEXT("first person on")); });
	At(3.55f, [this, Shot]() { Shot(TEXT("first person on 2")); });
	At(3.8f, [this, Shot]() { Shot(TEXT("first person on 3")); });
	At(4.0f, [this]() { Flashlight->Toggle(); });
	At(5.0f, [this, Shot]() { Shot(TEXT("first person off")); });
	At(5.6f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugGuideShots()
{
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	TArray<FVector> Lamps;
	for (TActorIterator<ANLGuideLanterns> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UPointLightComponent*> Lights(*It);
		for (UPointLightComponent* L : Lights) Lamps.Add(L->GetComponentLocation());
	}
	UE_LOG(LogTemp, Log, TEXT("NLGuideShots: %d lamps"), Lamps.Num());
	if (Lamps.Num() < 2) return;
	auto Ground = [this](FVector P)
	{
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NLGuideShot), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 1000.f), P - FVector(0, 0, 2000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		return P;
	};
	auto View = [this](const FVector& Loc, const FVector& Look, const FString& Name)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) return;
		if (!DebugCamera) DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		DebugCamera->SetActorLocationAndRotation(Loc, (Look - Loc).Rotation());
		PC->SetViewTarget(DebugCamera);
		UE_LOG(LogTemp, Log, TEXT("NLGuideShots: %s"), *Name);
	};
	auto Shot = [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot")); };
	if (Flashlight->IsOn()) Flashlight->Toggle();
	float T = 1.5f;
	for (int32 i = 1; i < Lamps.Num(); ++i)
	{
		// Önceki fenerle bu fenerin arasında, yolun ortasına yakın bir yerden (göz hizası) bu fenere bak
		const FVector From = Ground(FMath::Lerp(Lamps[i - 1], Lamps[i], 0.35f)) + FVector(0, 0, 165.f);
		const FVector To = Ground(Lamps[i]) + FVector(0, 0, 120.f);
		At(T, [View, From, To, i]() { View(From, To, FString::Printf(TEXT("walk toward lamp %d"), i)); });
		At(T + 0.6f, Shot);
		T += 1.2f;
	}
	FVector C = FVector::ZeroVector;
	for (const FVector& L : Lamps) C += L;
	C /= Lamps.Num();
	At(T, [View, C]() { View(C + FVector(-1800.f, -1800.f, 2600.f), C, TEXT("overview")); });
	At(T + 0.6f, Shot);
	// Titreme: aynı fenere yakından birkaç kare
	const FVector L0 = Lamps[1];
	const FVector Near = Ground(FMath::Lerp(Lamps[0], Lamps[1], 0.7f)) + FVector(0, 0, 165.f);
	for (int32 k = 0; k < 6; ++k)
	{
		At(T + 1.2f + k * 0.15f, [View, Near, L0, k]() { View(Near, L0, FString::Printf(TEXT("flicker check %d"), k)); });
		At(T + 1.25f + k * 0.15f, Shot);
	}
	At(T + 3.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugStutter()
{
	if (!Flashlight->IsOn()) Flashlight->Toggle();
	Flashlight->SetStutterInterval(FVector2D(0.6f, 1.2f));
	ANLLighthouse* LH = nullptr;
	for (TActorIterator<ANLLighthouse> It(GetWorld()); It; ++It) { LH = *It; LH->SetStutterInterval(FVector2D(0.6f, 1.2f)); break; }
	static int32 N = 0;
	N = 0;
	static FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, LH]()
	{
		float LHI = -1.f;
		if (LH) { TInlineComponentArray<USpotLightComponent*> S(LH); if (S.Num()) LHI = S[0]->Intensity; }
		UE_LOG(LogTemp, Log, TEXT("NLStutter: t=%.2f flashlight=%.0f lighthouse=%.0f"), GetWorld()->GetTimeSeconds(), Flashlight->Intensity, LHI);
		if (++N > 120) { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }
	}), 0.05f, true, 1.f);
}

extern RENDERCORE_API uint32 GRenderThreadTime;
extern RENDERCORE_API uint32 GGameThreadTime;

void ANLPlayerCharacter::NLDebugPerf()
{
	struct FStop { const TCHAR* Name; float X, Y, Pitch, Yaw; };
	static const FStop Stops[] = {
		{ TEXT("dock"),            -2850.f, 2950.f, 0.f, -60.f },
		{ TEXT("path to lighthouse"), -1500.f, 2000.f, 2.f, -40.f },
		{ TEXT("forest"),           1582.f, -2500.f, 2.f, -90.f },
		{ TEXT("lighthouse yard"),    54.f, -500.f, 6.f, -60.f },
		{ TEXT("inside cabin"),    -2819.f, 3195.f, 4.f, -127.f },
	};
	if (!Flashlight->IsOn()) Flashlight->Toggle();
	FTimerManager& TM = GetWorldTimerManager();
	const float Step = 6.f;
	for (int32 i = 0; i < UE_ARRAY_COUNT(Stops); ++i)
	{
		const FStop S = Stops[i];
		const float T0 = 2.f + i * Step;
		FTimerHandle H1, H2, H3;
		TM.SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NLPerf), false, this);
			float Z = GetActorLocation().Z;
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(S.X, S.Y, 5000.f), FVector(S.X, S.Y, -2000.f), ECC_Visibility, Q)) Z = Hit.ImpactPoint.Z + 100.f;
			SetActorLocation(FVector(S.X, S.Y, Z), false, nullptr, ETeleportType::TeleportPhysics);
			if (Controller) Controller->SetControlRotation(FRotator(S.Pitch, S.Yaw, 0.f));
		}), T0, false);
		// 2.5 sn otursun, sonra 2 sn boyunca ölç
		TM.SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			struct FAcc { double Frame = 0, Game = 0, Draw = 0, Gpu = 0; int32 N = 0; double Start = 0; };
			TSharedRef<FAcc> A = MakeShared<FAcc>();
			A->Start = FPlatformTime::Seconds();
			const FString Name = S.Name;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([A, Name](float Dt)
			{
				A->Frame += Dt * 1000.0;
				A->Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
				A->Draw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
				A->Gpu += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
				A->N++;
				if (FPlatformTime::Seconds() - A->Start < 2.0) return true;
				UE_LOG(LogTemp, Log, TEXT("NLPerf: %-20s frame %.1f ms (%.0f fps) | game %.1f | draw %.1f | gpu %.1f"),
					*Name, A->Frame / A->N, 1000.0 / (A->Frame / A->N), A->Game / A->N, A->Draw / A->N, A->Gpu / A->N);
				return false;
			}));
		}), T0 + 2.5f, false);
		if (i == 3)
		{
			FTimerHandle H4;
			TM.SetTimer(H4, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("stat dumpframe -ms=0.25 -root=GameThread"));
			}), T0 + 4.5f, false);
		}
		if (i == 2 || i == 4)
		{
			TM.SetTimer(H3, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("ProfileGPU"));
			}), T0 + 5.f, false);
		}
	}
	FTimerHandle HQ;
	TM.SetTimer(HQ, FTimerDelegate::CreateWeakLambda(this, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); }), 2.f + UE_ARRAY_COUNT(Stops) * Step + 1.f, false);
}

void ANLPlayerCharacter::NLDebugGenShots()
{
	ANLGenerator* G = nullptr;
	for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It) { G = *It; break; }
	if (!G) return;
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto View = [this, G](const FVector& LocalCam, const FVector& LocalLook)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) return;
		if (!DebugCamera) DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		const FVector Cam = G->GetActorTransform().TransformPosition(LocalCam);
		const FVector Look = G->GetActorTransform().TransformPosition(LocalLook);
		DebugCamera->SetActorLocationAndRotation(Cam, (Look - Cam).Rotation());
		PC->SetViewTarget(DebugCamera);
		// Oyuncu kameranın yanında, fener açık ve jeneratöre doğru (ışık sahneyi aydınlatsın)
		SetActorLocation(Cam + FVector(0, 0, -60.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation((Look - Cam).Rotation());
	};
	auto Shot = [this](const TCHAR* Name) { UE_LOG(LogTemp, Log, TEXT("NLGenShots: %s"), Name); if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot")); };
	At(0.5f, [this]() { if (!Flashlight->IsOn()) Flashlight->Toggle(); });
	// Sadece bu test karesi için: modeli görmek adına geçici dolgu ışığı
	At(0.9f, [this, G]() { UPointLightComponent* Fill = NewObject<UPointLightComponent>(G); Fill->SetupAttachment(G->GetRootComponent()); Fill->RegisterComponent(); Fill->SetRelativeLocation(FVector(250.f, -420.f, 320.f)); Fill->SetIntensityUnits(ELightUnits::Candelas); Fill->SetIntensity(60.f); Fill->SetAttenuationRadius(1400.f); Fill->SetCastShadows(false); Fill->ComponentTags.Add(TEXT("NLDebugFill")); });
	At(1.0f, [View]() { View(FVector(380.f, -520.f, 190.f), FVector(0.f, 0.f, 110.f)); });
	At(2.35f, [G]() { TInlineComponentArray<UPointLightComponent*> Ls(G); for (UPointLightComponent* L : Ls) if (L->ComponentHasTag(TEXT("NLDebugFill"))) L->DestroyComponent(); });
	At(2.2f, [Shot]() { Shot(TEXT("generator 3/4 view")); });
	At(2.5f, [View]() { View(FVector(-60.f, -330.f, 170.f), FVector(-80.f, -76.f, 125.f)); });
	At(3.7f, [Shot]() { Shot(TEXT("control door + fuel inlet")); });
	At(3.9f, [G]() { G->BreakDown(); });
	At(4.6f, [Shot]() { Shot(TEXT("broken: red lamp")); });
	At(4.85f, [Shot]() { Shot(TEXT("broken: red lamp (blink)")); });
	At(5.2f, [View]() { View(FVector(560.f, 120.f, 200.f), FVector(150.f, 0.f, 110.f)); });
	At(6.4f, [Shot]() { Shot(TEXT("radiator end")); });
	At(7.0f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}

void ANLPlayerCharacter::NLDebugClimb()
{
	ANLLighthouse* LH = nullptr;
	for (TActorIterator<ANLLighthouse> It(GetWorld()); It; ++It) { LH = *It; break; }
	if (!LH) return;
	const FTransform X = LH->GetActorTransform();
	const float HH = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Sz = 1.f;   // aktörün kendi ölçeği; kule ölçeği yerel Z'de
	// Merdiven başı: zemin katında, ilk basamağın önü
	// Kulenin dış kapısının önünden başla (kapı 300 derecede, evin girişinden ayrı)
	const float A0 = FMath::DegreesToRadians(300.f);
	FVector Start = X.TransformPosition(FVector(FMath::Cos(A0) * 400.f, FMath::Sin(A0) * 400.f, 0.f));
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLClimb), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start + FVector(0, 0, 200.f), Start - FVector(0, 0, 300.f), ECC_Visibility, Q)) Start.Z = Hit.ImpactPoint.Z;
	SetActorLocation(Start + FVector(0, 0, HH + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
	// Kapıları aç (kule kapısı kapalı başlar)
	for (TActorIterator<ANLSwingDoor> It(GetWorld()); It; ++It) It->SetOpen(true, true);
	if (!Flashlight->IsOn()) Flashlight->Toggle();
	UE_LOG(LogTemp, Log, TEXT("NLClimb: start at z %.0f (lighthouse z %.0f), capsule r %.0f hh %.0f"), GetActorLocation().Z, X.GetLocation().Z, GetCapsuleComponent()->GetScaledCapsuleRadius(), HH);

	struct FState { float T = 0.f; float LastZ = 0.f; float StuckT = 0.f; int32 Phase = -1; int32 Shots = 0; };
	TSharedRef<FState> S = MakeShared<FState>();
	S->LastZ = GetActorLocation().Z;
	static FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, X, S, HH]()
	{
		const float Dt = 0.02f;
		S->T += Dt;
		const FVector L = X.InverseTransformPosition(GetActorLocation());
		const float A = FMath::Atan2(L.Y, L.X);
		const float R = FVector2D(L.X, L.Y).Size();
		auto Shot = [this, S](const TCHAR* Name) { UE_LOG(LogTemp, Log, TEXT("NLClimb: shot %s"), Name); if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot")); };
		if (S->Phase == -1)
		{
			// Kapıdan içeri yürü
			const float DA = FMath::DegreesToRadians(300.f);
			const FVector Goal(FMath::Cos(DA) * 120.f, FMath::Sin(DA) * 120.f, L.Z);
			const FVector D = (Goal - L).GetSafeNormal2D();
			AddMovementInput(X.TransformVectorNoScale(D), 1.f);
			if (Controller) Controller->SetControlRotation(FRotator(-5.f, X.TransformVectorNoScale(D).Rotation().Yaw, 0.f));
			if (S->Shots == 0 && S->T > 0.3f) { S->Shots = -1; Shot(TEXT("tower door from outside")); }
			if (R < 150.f) { UE_LOG(LogTemp, Log, TEXT("NLClimb: entered the tower through its own door at t=%.1f s"), S->T); S->Phase = 0; S->T = 0.f; S->Shots = 0; S->LastZ = GetActorLocation().Z; }
			else if (S->T > 12.f) { UE_LOG(LogTemp, Warning, TEXT("NLClimb: could not enter the tower (r %.0f)"), R); Shot(TEXT("stuck at door")); S->Phase = 9; S->T = 0.f; }
		}
		else if (S->Phase == 0)
		{
			// Merdiveni saat yönünün tersine tırman (yarıçap ~105)
			const FVector Tan(-FMath::Sin(A), FMath::Cos(A), 0.f);
			const FVector Rad(FMath::Cos(A), FMath::Sin(A), 0.f);
			const FVector Dir = (Tan + Rad * FMath::Clamp((92.f - R) * 0.02f, -0.5f, 0.5f)).GetSafeNormal();
			const FVector W = X.TransformVectorNoScale(Dir);
			AddMovementInput(W, 1.f);
			if (Controller) Controller->SetControlRotation(FRotator(18.f, W.Rotation().Yaw, 0.f));
			if (FMath::Fmod(S->T, 1.f) < Dt) UE_LOG(LogTemp, Log, TEXT("NLClimb: t=%.0f z=%.0f (local %.0f) angle=%.0f r=%.0f"), S->T, GetActorLocation().Z, L.Z, FMath::RadiansToDegrees(A), R);
			if (S->Shots == 0 && L.Z > 450.f) { S->Shots = 1; Shot(TEXT("on the stairs")); }
			// Takıldı mı
			if (GetActorLocation().Z > S->LastZ + 5.f) { S->LastZ = GetActorLocation().Z; S->StuckT = 0.f; }
			else S->StuckT += Dt;
			if (L.Z > 1000.f + HH)
			{
				UE_LOG(LogTemp, Log, TEXT("NLClimb: reached the lantern room at t=%.1f s (local z %.0f)"), S->T, L.Z);
				S->Phase = 1; S->T = 0.f;
			}
			else if (S->StuckT > 4.f || S->T > 90.f)
			{
				UE_LOG(LogTemp, Warning, TEXT("NLClimb: STUCK at local z %.0f angle %.0f r %.0f"), L.Z, FMath::RadiansToDegrees(A), R);
				Shot(TEXT("stuck"));
				S->Phase = 9; S->T = 0.f;
			}
		}
		else if (S->Phase == 1)
		{
			// Merdiven boşluğundan çık: zemindeki açık tarafa (330 derece) yürü, lense bak
			const FVector Goal(FMath::Cos(FMath::DegreesToRadians(320.f)) * 88.f, FMath::Sin(FMath::DegreesToRadians(320.f)) * 88.f, L.Z);
			const FVector D = (Goal - L).GetSafeNormal2D();
			if ((Goal - L).Size2D() > 15.f && S->T < 4.f) AddMovementInput(X.TransformVectorNoScale(D), 1.f);
			const FVector LensW = X.TransformPosition(FVector(0, 0, L.Z + 60.f));
			if (Controller) Controller->SetControlRotation((LensW - FirstPersonCamera->GetComponentLocation()).Rotation());
			if (S->T > 4.5f) { Shot(TEXT("lantern room, next to the lens")); S->Phase = 2; S->T = 0.f; }
		}
		else if (S->Phase == 2)
		{
			// Kapıdan (+X) balkona çık, sonra balkonda yürü
			const FVector Goal = S->T < 2.5f ? FVector(105.f, 0.f, L.Z) : (S->T < 4.5f ? FVector(212.f, 0.f, L.Z) : FVector(FMath::Cos(0.9f) * 212.f, FMath::Sin(0.9f) * 212.f, L.Z));
			const FVector D = (Goal - L).GetSafeNormal2D();
			if ((Goal - L).Size2D() > 12.f) AddMovementInput(X.TransformVectorNoScale(D), 1.f);
			if (Controller) Controller->SetControlRotation(FRotator(-6.f, X.TransformVectorNoScale(D).Rotation().Yaw, 0.f));
			if (S->T > 7.f)
			{
				UE_LOG(LogTemp, Log, TEXT("NLClimb: on the gallery at local (%.0f, %.0f, %.0f) r=%.0f"), L.X, L.Y, L.Z, R);
				Shot(TEXT("gallery"));
				S->Phase = 3; S->T = 0.f;
			}
		}
		else if (S->Phase == 3)
		{
			// Balkondan fener odasına geri bak
			const FVector LensW = X.TransformPosition(FVector(0, 0, L.Z + 40.f));
			if (Controller) Controller->SetControlRotation((LensW - FirstPersonCamera->GetComponentLocation()).Rotation());
			if (S->T > 1.f) { Shot(TEXT("gallery, looking at the lantern")); S->Phase = 9; S->T = 0.f; }
		}
		else if (S->T > 1.5f)
		{
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit"));
		}
	}), 0.02f, true, 1.0f);
}

void ANLPlayerCharacter::NLDebugLHViews()
{
	ANLLighthouse* LH = nullptr;
	for (TActorIterator<ANLLighthouse> It(GetWorld()); It; ++It) { LH = *It; break; }
	if (!LH) return;
	const FTransform X = LH->GetActorTransform();
	FTimerManager& TM = GetWorldTimerManager();
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto View = [this, X](const FVector& LocalCam, const FVector& LocalLook, const FString& Name)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC) return;
		if (!DebugCamera) DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		FVector Cam = X.TransformPosition(LocalCam);
		// Kamerayı zeminin 170 cm üstüne oturt
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NLLHView), false, this);
		if (LocalCam.Z < 0.f && GetWorld()->LineTraceSingleByChannel(Hit, Cam + FVector(0, 0, 3000.f), Cam - FVector(0, 0, 3000.f), ECC_Visibility, Q)) Cam.Z = Hit.ImpactPoint.Z + 170.f;
		const FVector Look = X.TransformPosition(LocalLook);
		DebugCamera->SetActorLocationAndRotation(Cam, (Look - Cam).Rotation());
		PC->SetViewTarget(DebugCamera);
		// Fener ışığı sahneyi aydınlatsın diye oyuncu kameranın yanında
		SetActorLocation(Cam + FVector(0, 0, -60.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (Controller) Controller->SetControlRotation((Look - Cam).Rotation());
		UE_LOG(LogTemp, Log, TEXT("NLLHViews: %s"), *Name);
	};
	auto Shot = [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot")); };
	At(0.5f, [this]() { if (!Flashlight->IsOn()) Flashlight->Toggle(); });
	float T = 1.2f;
	for (int32 k = 0; k < 8; ++k)
	{
		const float A = FMath::DegreesToRadians(k * 45.f + 20.f);
		const FVector Cam(400.f + FMath::Cos(A) * 1000.f, FMath::Sin(A) * 1000.f, -1.f);
		At(T, [View, Cam, k]() { View(Cam, FVector(380.f, 0.f, 120.f), FString::Printf(TEXT("around %d"), k)); });
		At(T + 0.8f, Shot);
		T += 1.3f;
	}
	// Kapılar: kapalı ve açık
	At(T, [View]() { View(FVector(670.f, -560.f, -1.f), FVector(670.f, -235.f, 160.f), TEXT("house door closed")); });
	At(T + 0.8f, Shot);
	At(T + 1.0f, [this]() { for (TActorIterator<ANLSwingDoor> It(GetWorld()); It; ++It) It->Interact(this); });
	At(T + 2.0f, Shot);
	T += 2.4f;
	At(T, [View]() { View(FVector(700.f, 120.f, 210.f), FVector(300.f, -60.f, 80.f), TEXT("house inside")); });
	At(T + 0.8f, Shot);
	At(T + 1.3f, [View]() { View(FVector(-60.f, 60.f, 200.f), FVector(150.f, -150.f, 20.f), TEXT("tower inside floor")); });
	At(T + 2.1f, Shot);
	At(T + 3.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}
