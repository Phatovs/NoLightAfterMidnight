#include "NLPowerSubsystem.h"
#include "NLGenerator.h"
#include "NLFuseBox.h"

void UNLPowerSubsystem::RegisterGenerator(ANLGenerator* InGenerator)
{
	Generator = InGenerator;
	RefreshPower();
}

void UNLPowerSubsystem::RegisterFuseBox(ANLFuseBox* InFuseBox)
{
	FuseBox = InFuseBox;
	RefreshPower();
}

void UNLPowerSubsystem::RefreshPower()
{
	const bool bGeneratorOk = Generator.IsValid() && Generator->IsProducingPower();
	// Sahnede fuse yoksa jeneratör doğrudan besler
	const bool bFuseOk = !FuseBox.IsValid() || FuseBox->IsPowerOk();
	const bool bNewPower = bGeneratorOk && bFuseOk;

	if (bNewPower != bHasPower)
	{
		bHasPower = bNewPower;
		UE_LOG(LogTemp, Log, TEXT("NLPower: power %s"), bHasPower ? TEXT("ON") : TEXT("OFF"));
		OnPowerChanged.Broadcast(bHasPower);
	}
}

void UNLPowerSubsystem::MarkFailureActive()
{
	LastFailureTime = GetWorld()->GetTimeSeconds();
}

bool UNLPowerSubsystem::CanStartFailure(float& OutWait) const
{
	const float Since = GetWorld()->GetTimeSeconds() - LastFailureTime;
	OutWait = FMath::Max(0.f, MinFailureGap - Since);
	return OutWait <= 0.f;
}
