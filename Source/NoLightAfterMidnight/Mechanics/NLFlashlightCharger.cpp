#include "NLFlashlightCharger.h"
#include "NLDefaults.h"
#include "NLFlashlightComponent.h"
#include "NLPowerConsumerComponent.h"
#include "NLPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"

ANLFlashlightCharger::ANLFlashlightCharger()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(DockSound, "charger_dock");
	NL_DEFAULT_SOUND(UndockSound, "charger_dock");
	NL_DEFAULT_SOUND(FullSound, "charge_full");

	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(Mesh);
	IndicatorLight->SetAttenuationRadius(40.f);
	IndicatorLight->SetIntensity(0.f);

	PowerConsumer = CreateDefaultSubobject<UNLPowerConsumerComponent>(TEXT("PowerConsumer"));
}

void ANLFlashlightCharger::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UNLFlashlightComponent* Light = DockedFlashlight.Get();
	if (Light && PowerConsumer->IsPowered())
	{
		Light->AddCharge(ChargePerSecond * DeltaSeconds);
		const bool bFull = Light->GetBatteryPercent() >= 1.f;
		if (bFull && !bWasFull)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FullSound, GetActorLocation());
		}
		bWasFull = bFull;
	}
	UpdateIndicator();
}

void ANLFlashlightCharger::Interact(ANLPlayerCharacter* Player)
{
	if (!Player) return;
	UNLFlashlightComponent* PlayerLight = Player->GetFlashlight();

	if (DockedFlashlight.IsValid())
	{
		DockedFlashlight->SetDocked(false);
		DockedFlashlight.Reset();
		UGameplayStatics::PlaySoundAtLocation(this, UndockSound, GetActorLocation());
	}
	else if (PlayerLight)
	{
		PlayerLight->SetDocked(true);
		DockedFlashlight = PlayerLight;
		bWasFull = PlayerLight->GetBatteryPercent() >= 1.f;
		UGameplayStatics::PlaySoundAtLocation(this, DockSound, GetActorLocation());
	}
}

FText ANLFlashlightCharger::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	if (DockedFlashlight.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("[E] Feneri al (Pil: %%%d)%s"),
			FMath::RoundToInt(DockedFlashlight->GetBatteryPercent() * 100.f),
			PowerConsumer->IsPowered() ? TEXT("") : TEXT(" - ELEKTRİK YOK")));
	}
	return FText::FromString(TEXT("[E] Feneri şarja tak"));
}

void ANLFlashlightCharger::UpdateIndicator()
{
	if (!PowerConsumer->IsPowered() || !DockedFlashlight.IsValid())
	{
		IndicatorLight->SetIntensity(0.f);
		return;
	}
	const bool bFull = DockedFlashlight->GetBatteryPercent() >= 1.f;
	IndicatorLight->SetLightColor(bFull ? FLinearColor::Green : FLinearColor::Red);
	IndicatorLight->SetIntensity(15.f);
}
