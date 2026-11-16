#include "NLMenuDirector.h"
#include "NLDefaults.h"
#include "NLGameSubsystem.h"
#include "NLMenuWidgets.h"
#include "NLLighthouse.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ANLMenuGameMode::ANLMenuGameMode()
{
	// Menüde oyuncu karakteri yok: kamera ANLMenuDirector'ın
	DefaultPawnClass = nullptr;
}

ANLMenuDirector::ANLMenuDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetFieldOfView(80.f);

	// Elde tutulan el feneri: kameranın sağ altında
	FlashPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FlashPivot"));
	FlashPivot->SetupAttachment(Camera);
	FlashPivot->SetRelativeLocation(FVector(62.f, 21.f, -17.f));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Metal(TEXT("/Game/Environment/Materials/MI_NL_Metal.MI_NL_Metal"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Lens(TEXT("/Game/Environment/Materials/MI_NL_LanternGlass.MI_NL_LanternGlass"));
	FlashBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashBody"));
	FlashBody->SetupAttachment(FlashPivot);
	FlashBody->SetStaticMesh(Cyl.Get());
	FlashBody->SetRelativeLocationAndRotation(FVector(-8.f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
	FlashBody->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.2f));
	FlashBody->SetCastShadow(false);
	FlashBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Metal.Get()) FlashBody->SetMaterial(0, Metal.Get());
	FlashHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashHead"));
	FlashHead->SetupAttachment(FlashPivot);
	FlashHead->SetStaticMesh(Cyl.Get());
	FlashHead->SetRelativeLocationAndRotation(FVector(4.f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
	FlashHead->SetRelativeScale3D(FVector(0.055f, 0.055f, 0.04f));
	FlashHead->SetCastShadow(false);
	FlashHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Metal.Get()) FlashHead->SetMaterial(0, Metal.Get());

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FlashPivot);
	Flashlight->SetRelativeLocation(FVector(8.f, 0.f, 0.f));
	Flashlight->SetAttenuationRadius(1800.f);
	Flashlight->SetInnerConeAngle(11.f);
	Flashlight->SetOuterConeAngle(24.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Flashlight->SetVolumetricScatteringIntensity(2.5f);
	Flashlight->SetCastShadows(true);

	Lightning = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lightning"));
	Lightning->SetupAttachment(RootComponent);
	Lightning->SetUsingAbsoluteLocation(true);
	Lightning->SetIntensity(0.f);
	Lightning->SetAttenuationRadius(2000.f);
	Lightning->SetLightColor(FLinearColor(0.7f, 0.8f, 1.f));
	Lightning->SetCastShadows(true);

	NL_DEFAULT_SOUND(RainLoop, "amb_rain_loop");
	NL_DEFAULT_SOUND(RoofLoop, "amb_rain_roof_loop");
	NL_DEFAULT_SOUND(WavesLoop, "amb_waves_loop");
	NL_DEFAULT_SOUND(WindLoop, "amb_wind_gust_loop");
	static const TCHAR* ThunderPaths[] = { TEXT("/Game/Audio/NL/thunder_far.thunder_far"), TEXT("/Game/Audio/NL/thunder_far2.thunder_far2"), TEXT("/Game/Audio/NL/thunder_near.thunder_near") };
	for (const TCHAR* P : ThunderPaths)
	{
		ConstructorHelpers::FObjectFinderOptional<USoundBase> F(P);
		if (F.Get()) Thunder.Add(F.Get());
	}
}

void ANLMenuDirector::BeginPlay()
{
	Super::BeginPlay();
	Flashlight->SetIntensity(FlashlightIntensity);
	Lightning->SetWorldLocation(LightningLocation);

	if (UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this)) Sub->ApplySettings(this);

	for (TActorIterator<ANLLighthouse> It(GetWorld()); It; ++It) { Lighthouse = *It; break; }
	if (!bHandFlashlight)
	{
		FlashBody->SetVisibility(false);
		FlashHead->SetVisibility(false);
		Flashlight->SetVisibility(false);
	}
	if (bIndoor)
	{
		RainAudio = UGameplayStatics::SpawnSound2D(this, RainLoop, 0.25f);
		if (RainAudio) { RainAudio->SetLowPassFilterEnabled(true); RainAudio->SetLowPassFilterFrequency(900.f); }
		RoofAudio = UGameplayStatics::SpawnSound2D(this, RoofLoop, 0.55f);
	}
	else
	{
		// Dışarıda: dalgalar ve rüzgâr (yağmur, şimşek ve gök gürültüsü hava yöneticisinden)
		RainAudio = UGameplayStatics::SpawnSound2D(this, WavesLoop, 0.7f);
		RoofAudio = UGameplayStatics::SpawnSound2D(this, WindLoop, 0.45f);
		LightningTimer = 1.0e9f;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTarget(this);
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.5f, FLinearColor::Black, true, false);
		Menu = CreateWidget<UNLMainMenuWidget>(PC, UNLMainMenuWidget::StaticClass());
		if (Menu)
		{
			Menu->AddToViewport(10);
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(Menu->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(Mode);
		}
	}
	AimRot = Camera->GetComponentRotation();
}

void ANLMenuDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;

	// Fener imlecin gösterdiği yere döner
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FVector Loc, Dir;
		bool bHave = false;
		if (NLMenuDebug::Cursor.IsSet())
		{
			int32 VX = 0, VY = 0;
			PC->GetViewportSize(VX, VY);
			bHave = PC->DeprojectScreenPositionToWorld(VX * NLMenuDebug::Cursor->X, VY * NLMenuDebug::Cursor->Y, Loc, Dir);
		}
		else bHave = PC->DeprojectMousePositionToWorld(Loc, Dir);
		if (bHave)
		{
			FHitResult Hit;
			FVector Target = Loc + Dir * 600.f;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(NLMenuAim), false, this);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Loc, Loc + Dir * 3000.f, ECC_Visibility, Q)) Target = Hit.ImpactPoint;
			const FRotator Want = (Target - FlashPivot->GetComponentLocation()).Rotation();
			AimRot = FMath::RInterpTo(AimRot, Want, DeltaSeconds, AimSpeed);
			// Elde tutulduğu için hafif titreme
			const FRotator Sway(FMath::Sin(Time * 1.3f) * 0.4f, FMath::Sin(Time * 0.9f + 1.f) * 0.5f, 0.f);
			FlashPivot->SetWorldRotation(AimRot + Sway);
			if (ANLLighthouse* LH = Lighthouse.Get()) LH->AimAt(Loc + Dir * LighthouseAimDistance, 4.f);
		}
	}
	// Dışarıda kamera dalgada hafifçe sallanır
	if (!bIndoor) Camera->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 6.f * FMath::Sin(Time * 0.7f)), FRotator(1.2f * FMath::Sin(Time * 0.5f), 0.f, 0.9f * FMath::Sin(Time * 0.6f + 1.f)));

	// Ara ara fener titrer
	const float Flick = (FMath::Fmod(Time, 11.f) < 0.12f) ? FMath::FRandRange(0.2f, 0.9f) : 1.f;
	Flashlight->SetIntensity(FlashlightIntensity * Flick);

	// Şimşek: pencereden bir an aydınlanır, biraz sonra gök gürler
	LightningTimer -= DeltaSeconds;
	if (LightningTimer <= 0.f)
	{
		LightningTimer = FMath::FRandRange(LightningGapMin, LightningGapMax);
		FlashT = 0.f;
		ThunderIn = FMath::FRandRange(0.8f, 2.5f);
	}
	if (FlashT >= 0.f)
	{
		FlashT += DeltaSeconds;
		const float A = FMath::Max(FMath::Exp(-FlashT / 0.06f), FlashT > 0.18f ? 0.7f * FMath::Exp(-(FlashT - 0.18f) / 0.15f) : 0.f);
		Lightning->SetIntensity(60000.f * A);
		if (FlashT > 1.2f) { FlashT = -1.f; Lightning->SetIntensity(0.f); }
	}
	if (ThunderIn >= 0.f)
	{
		ThunderIn -= DeltaSeconds;
		if (ThunderIn < 0.f && Thunder.Num() > 0)
		{
			if (UAudioComponent* T = UGameplayStatics::SpawnSound2D(this, Thunder[FMath::RandRange(0, Thunder.Num() - 1)], 0.6f))
			{
				T->SetLowPassFilterEnabled(true);
				T->SetLowPassFilterFrequency(1200.f);
			}
		}
	}
}

void ANLMenuGameMode::NLDebugMenuShots()
{
	struct FStep { float T; FVector2D C; const TCHAR* Name; int32 Settings; };
	static const FStep Steps[] = {
		{ 3.0f, FVector2D(0.15f, 0.47f), TEXT("new game"), 0 },
		{ 5.0f, FVector2D(0.15f, 0.62f), TEXT("settings item"), 0 },
		{ 7.0f, FVector2D(0.62f, 0.42f), TEXT("light on the wall"), 0 },
		{ 9.0f, FVector2D(0.2f, 0.2f), TEXT("title"), 0 },
		{ 11.0f, FVector2D(0.5f, 0.5f), TEXT("settings panel"), 1 },
	};
	for (const FStep& S : Steps)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this, S]()
		{
			NLMenuDebug::Cursor = S.C;
			if (S.Settings)
			{
				for (TActorIterator<ANLMenuDirector> It(GetWorld()); It; ++It) { if (It->GetMenu()) It->GetMenu()->DebugShowSettings(true); }
			}
			FTimerHandle H2;
			GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [this, S]()
			{
				UE_LOG(LogTemp, Log, TEXT("NLMenuShots: %s"), S.Name);
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ConsoleCommand(TEXT("shot showui"));
			}), 1.2f, false);
		}), S.T, false);
	}
	FTimerHandle Q;
	GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ConsoleCommand(TEXT("quit"));
	}), 14.f, false);
}
