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
	const bool bFuseOk = !FuseBox.IsValid() || FuseBox->IsOn();
	const bool bNewPower = bGeneratorOk && bFuseOk;

	if (bNewPower != bHasPower)
	{
		bHasPower = bNewPower;
		OnPowerChanged.Broadcast(bHasPower);
	}
}
