// NoLightAfterMidnight - Elektrik şebekesi: Jeneratör -> Fuse -> Tüketiciler
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NLTypes.h"
#include "NLPowerSubsystem.generated.h"

class ANLGenerator;
class ANLFuseBox;

/**
 * Tek bir doğruluk kaynağı: "Adada elektrik var mı?"
 * Elektrik = jeneratör çalışıyor (yakıt > 0 ve arızalı değil) VE fuse açık.
 * Durum değiştiğinde OnPowerChanged yayınlanır; kamera, ışık, şarj ve bilgisayar dinler.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLPowerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterGenerator(ANLGenerator* InGenerator);
	void RegisterFuseBox(ANLFuseBox* InFuseBox);

	// Jeneratör/fuse durumu değişince çağrılır
	void RefreshPower();

	UFUNCTION(BlueprintPure, Category = "NL|Power")
	bool HasPower() const { return bHasPower; }

	UFUNCTION(BlueprintPure, Category = "NL|Power")
	ANLGenerator* GetGenerator() const { return Generator.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "NL|Power")
	FNLOnPowerChanged OnPowerChanged;

private:
	TWeakObjectPtr<ANLGenerator> Generator;
	TWeakObjectPtr<ANLFuseBox> FuseBox;
	bool bHasPower = false;
};
