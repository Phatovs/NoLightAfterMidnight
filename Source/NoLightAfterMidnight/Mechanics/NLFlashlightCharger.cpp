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
	NL_DEFAULT_SOUND(UndockSound, "charger_undock");
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
	TWeakObjectPtr<ANLFlashlightCharger> WeakThis(this);

	// Fener istasyonun üstünde yatar; oyuncu sağ eliyle oraya uzanır, el değdiği anda takılır/alınır
	const FBox Box = GetComponentsBoundingBox();
	const FVector DockPoint(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z + 3.f);
	const FRotator DockRotation(0.f, GetActorRotation().Yaw, 0.f);

	if (DockedFlashlight.IsValid())
	{
		Player->PlayHandAction(ENLHandAction::TakeFlashlight, DockPoint, DockRotation, HandActionDuration, 0.5f, [WeakThis]()
		{
			ANLFlashlightCharger* Self = WeakThis.Get();
			if (!Self || !Self->DockedFlashlight.IsValid()) return;
			Self->DockedFlashlight->SetDocked(false);
			Self->DockedFlashlight.Reset();
			UGameplayStatics::PlaySoundAtLocation(Self, Self->UndockSound, Self->GetActorLocation());
		});
	}
	else if (PlayerLight)
	{
		TWeakObjectPtr<UNLFlashlightComponent> WeakLight(PlayerLight);
		Player->PlayHandAction(ENLHandAction::DockFlashlight, DockPoint, DockRotation, HandActionDuration, 0.55f, [WeakThis, WeakLight]()
		{
			ANLFlashlightCharger* Self = WeakThis.Get();
			UNLFlashlightComponent* Light = WeakLight.Get();
			if (!Self || !Light) return;
			Light->SetDocked(true);
			Self->DockedFlashlight = Light;
			Self->bWasFull = Light->GetBatteryPercent() >= 1.f;
			UGameplayStatics::PlaySoundAtLocation(Self, Self->DockSound, Self->GetActorLocation());
		});
	}
}

FText ANLFlashlightCharger::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	if (DockedFlashlight.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("[E] Take flashlight (Battery: %d%%)%s"),
			FMath::RoundToInt(DockedFlashlight->GetBatteryPercent() * 100.f),
			PowerConsumer->IsPowered() ? TEXT("") : TEXT(" - NO POWER")));
	}
	return FText::FromString(TEXT("[E] Dock flashlight to charge"));
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
