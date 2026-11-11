// NoLightAfterMidnight - Elektrik tüketen her aktöre eklenen bileşen
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NLTypes.h"
#include "NLPowerConsumerComponent.generated.h"

/**
 * Kamera, ışık, fener şarjı ve bilgisayara takılır.
 * Elektrik gidince/gelince OnPowerStateChanged yayınlar.
 */
UCLASS(ClassGroup = (NL), meta = (BlueprintSpawnableComponent))
class NOLIGHTAFTERMIDNIGHT_API UNLPowerConsumerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNLPowerConsumerComponent();

	UFUNCTION(BlueprintPure, Category = "NL|Power")
	bool IsPowered() const { return bPowered; }

	UPROPERTY(BlueprintAssignable, Category = "NL|Power")
	FNLOnPowerChanged OnPowerStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandlePowerChanged(bool bHasPower);

private:
	bool bPowered = false;
};
