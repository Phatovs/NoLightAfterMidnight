#include "NLSecurityCamera.h"
#include "NLDefaults.h"
#include "NLPowerConsumerComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"

ANLSecurityCamera::ANLSecurityCamera()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(FlashSound, "light_flash");
	NL_DEFAULT_SOUND(FlashFailSound, "flash_fail");

	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CameraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraMesh"));
	CameraMesh->SetupAttachment(Root);
	CameraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(Root);
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->FOVAngle = 80.f;

	DefenseLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("DefenseLight"));
	DefenseLight->SetupAttachment(Root);
	DefenseLight->SetRelativeLocation(FVector(0.f, 0.f, -40.f)); // kameranın altı
	DefenseLight->SetIntensity(0.f);
	DefenseLight->SetAttenuationRadius(1800.f);
	DefenseLight->SetOuterConeAngle(40.f);
	DefenseLight->SetInnerConeAngle(20.f);
	DefenseLight->SetLightColor(FLinearColor(1.f, 0.96f, 0.88f));

	DrownedSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("DrownedSpawnPoint"));
	DrownedSpawnPoint->SetupAttachment(Root);
	DrownedSpawnPoint->SetRelativeLocation(FVector(500.f, 0.f, -250.f));
	DrownedSpawnPoint->SetRelativeRotation(FRotator(0.f, 180.f, 0.f)); // kameraya bakar
	DrownedSpawnPoint->ArrowColor = FColor::Red;

	PowerConsumer = CreateDefaultSubobject<UNLPowerConsumerComponent>(TEXT("PowerConsumer"));
}

void ANLSecurityCamera::BeginPlay()
{
	PowerConsumer->OnPowerStateChanged.AddDynamic(this, &ANLSecurityCamera::HandlePowerChanged);
	Super::BeginPlay();
	ApplyCCTVLook();
	HandlePowerChanged(PowerConsumer->IsPowered());
}

void ANLSecurityCamera::ApplyCCTVLook()
{
	if (!bApplyCCTVLook) return;

	FPostProcessSettings& PP = SceneCapture->PostProcessSettings;
	SceneCapture->PostProcessBlendWeight = 1.f;

	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.f, 1.f, 1.f, CCTVSaturation);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.f, 1.f, 1.f, CCTVContrast);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(CCTVTint.R, CCTVTint.G, CCTVTint.B, 1.f);
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = CCTVExposureBias;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = CCTVFilmGrain;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = CCTVVignette;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = CCTVBloom;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = CCTVChromaticAberration;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;
}

void ANLSecurityCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CooldownRemaining > 0.f)
	{
		CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaSeconds);
	}

	// Güçlü parlama + yavaş sönme
	if (FlashElapsed >= 0.f)
	{
		FlashElapsed += DeltaSeconds;
		float Alpha = 0.f;

		if (FlashElapsed < FlashRiseTime)
		{
			Alpha = FlashElapsed / FMath::Max(FlashRiseTime, KINDA_SMALL_NUMBER);
		}
		else
		{
			const float T = (FlashElapsed - FlashRiseTime) / FMath::Max(FlashFadeTime, KINDA_SMALL_NUMBER);
			Alpha = FMath::Square(FMath::Clamp(1.f - T, 0.f, 1.f));
			if (T >= 1.f)
			{
				FlashElapsed = -1.f;
			}
		}

		DefenseLight->SetIntensity(FlashPeakIntensity * Alpha);
	}
}

void ANLSecurityCamera::SetFeedActive(bool bActive, UTextureRenderTarget2D* Target)
{
	bFeedActive = bActive;
	SceneCapture->TextureTarget = bActive ? Target : nullptr;
	SceneCapture->bCaptureEveryFrame = bActive && PowerConsumer->IsPowered();

	if (SceneCapture->bCaptureEveryFrame)
	{
		SceneCapture->CaptureScene();
	}
}

bool ANLSecurityCamera::Flash()
{
	if (!PowerConsumer->IsPowered() || CooldownRemaining > 0.f)
	{
		UGameplayStatics::PlaySound2D(this, FlashFailSound);
		return false;
	}

	FlashElapsed = 0.f;
	CooldownRemaining = FlashCooldown;
	// Oyuncu masada, kamera uzakta: ses monitör hoparlöründen gelir gibi 2D çalınır
	UGameplayStatics::PlaySound2D(this, FlashSound);
	OnFlashed.Broadcast(this);
	return true;
}

FTransform ANLSecurityCamera::GetDrownedSpawnTransform() const
{
	return DrownedSpawnPoint->GetComponentTransform();
}

void ANLSecurityCamera::HandlePowerChanged(bool bHasPower)
{
	if (!bHasPower)
	{
		FlashElapsed = -1.f;
		DefenseLight->SetIntensity(0.f);
	}
	SceneCapture->bCaptureEveryFrame = bFeedActive && bHasPower;
}
