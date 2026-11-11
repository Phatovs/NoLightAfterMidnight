#include "NLPowerConsumerComponent.h"
#include "NLPowerSubsystem.h"

UNLPowerConsumerComponent::UNLPowerConsumerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNLPowerConsumerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->OnPowerChanged.AddDynamic(this, &UNLPowerConsumerComponent::HandlePowerChanged);
		HandlePowerChanged(Power->HasPower());
	}
}

void UNLPowerConsumerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UNLPowerSubsystem* Power = World->GetSubsystem<UNLPowerSubsystem>())
		{
			Power->OnPowerChanged.RemoveDynamic(this, &UNLPowerConsumerComponent::HandlePowerChanged);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UNLPowerConsumerComponent::HandlePowerChanged(bool bHasPower)
{
	bPowered = bHasPower;
	OnPowerStateChanged.Broadcast(bPowered);
}
