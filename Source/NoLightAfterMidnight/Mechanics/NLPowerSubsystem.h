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
	ANLFuseBox* GetFuseBox() const { return FuseBox.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "NL|Power")
	FNLOnPowerChanged OnPowerChanged;

	// --- Arıza kuralı: jeneratör arızası ve şalter atması aynı anda olmaz ---
	// Bir arıza sürerken (ya da bittikten sonra MinFailureGap sn boyunca) yenisi başlamaz.
	float MinFailureGap = 45.f;

	// Arızalı olan (bozuk jeneratör / atmış şalter) her karede çağırır
	void MarkFailureActive();
	// Yeni arıza başlayabilir mi? Başlayamıyorsa kaç sn beklemesi gerektiğini OutWait'e yazar.
	bool CanStartFailure(float& OutWait) const;

private:
	TWeakObjectPtr<ANLGenerator> Generator;
	TWeakObjectPtr<ANLFuseBox> FuseBox;
	bool bHasPower = false;
	float LastFailureTime = -1000.f;
};
