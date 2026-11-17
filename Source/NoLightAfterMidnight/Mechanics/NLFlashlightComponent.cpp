#include "NLFlashlightComponent.h"
#include "NLDefaults.h"
#include "Kismet/GameplayStatics.h"

UNLFlashlightComponent::UNLFlashlightComponent()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(ToggleSound, "flashlight_on");
	NL_DEFAULT_SOUND(OffSound, "flashlight_off");
	NL_DEFAULT_SOUND(EmptySound, "flashlight_empty");

	PrimaryComponentTick.bCanEverTick = true;
	SetIntensity(0.f);
	SetAttenuationRadius(1600.f);
	SetInnerConeAngle(12.f);
	SetOuterConeAngle(26.f);
	SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	// Sisli gecede fenerin huzmesi görünsün (hacimli sis gerekir) ve nesneler gölge atsın
	VolumetricScatteringIntensity = 2.5f;
	SetCastShadows(true);
}

void UNLFlashlightComponent::BeginPlay()
{
	Super::BeginPlay();
	Battery = FMath::Clamp(StartBattery, 0.f, 100.f);
	ApplyLight();
}

void UNLFlashlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Açık havada mı: yarım saniyede bir yukarı ışın (tavan var mı)
	OutdoorCheckTimer -= DeltaTime;
	if (OutdoorCheckTimer <= 0.f && GetOwner())
	{
		OutdoorCheckTimer = 0.5f;
		const FVector From = GetOwner()->GetActorLocation();
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NLFlashlightRoof), false, GetOwner());
		bOutdoors = !GetWorld()->LineTraceSingleByChannel(Hit, From, From + FVector(0.f, 0.f, RoofCheckHeight), ECC_Visibility, Q);
	}

	if (!bOn) return;

	Battery = FMath::Max(0.f, Battery - DrainPerSecond * (bOutdoors ? OutdoorDrainMultiplier : 1.f) * DeltaTime);
	if (Battery <= 0.f)
	{
		bOn = false;
		UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetComponentLocation());
		ApplyLight();
		return;
	}

	// Pil azalınca titreme
	if (GetBatteryPercent() < FlickerThreshold)
	{
		FlickerTimer -= DeltaTime;
		if (FlickerTimer <= 0.f)
		{
			FlickerTimer = FMath::FRandRange(0.03f, 0.25f);
			SetIntensity(OnIntensity * (FMath::FRand() < 0.3f ? 0.05f : FMath::FRandRange(0.4f, 1.f)));
		}
	}
	else
	{
		// Ara ara göz kırpar gibi tekler
		const float M = Stutter.Update(GetWorld()->GetTimeSeconds());
		if (!FMath::IsNearlyEqual(M, StutterMul))
		{
			StutterMul = M;
			SetIntensity(OnIntensity * M);
		}
	}
}

void UNLFlashlightComponent::Toggle()
{
	if (bDocked || (!bOn && Battery <= 0.f))
	{
		UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetComponentLocation());
		return;
	}
	bOn = !bOn;
	UGameplayStatics::PlaySoundAtLocation(this, bOn ? ToggleSound.Get() : (OffSound ? OffSound.Get() : ToggleSound.Get()), GetComponentLocation());
	ApplyLight();
}

void UNLFlashlightComponent::ForceOff()
{
	bOn = false;
	ApplyLight();
}

void UNLFlashlightComponent::SetDocked(bool bNewDocked)
{
	bDocked = bNewDocked;
	if (bDocked) bOn = false;
	ApplyLight();
}

void UNLFlashlightComponent::AddCharge(float Amount)
{
	Battery = FMath::Clamp(Battery + Amount, 0.f, 100.f);
}

void UNLFlashlightComponent::ApplyLight()
{
	StutterMul = 1.f;
	SetIntensity(bOn ? OnIntensity : 0.f);
}
