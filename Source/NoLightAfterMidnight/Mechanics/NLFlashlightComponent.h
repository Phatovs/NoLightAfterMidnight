// NoLightAfterMidnight - Oyuncunun feneri (pil + titreme)
#pragma once

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "NLFlashlightComponent.generated.h"

class USoundBase;

UCLASS(ClassGroup = (NL), meta = (BlueprintSpawnableComponent))
class NOLIGHTAFTERMIDNIGHT_API UNLFlashlightComponent : public USpotLightComponent
{
	GENERATED_BODY()

public:
	UNLFlashlightComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Toggle();
	void ForceOff();

	// Şarj istasyonuna takıldı mı (takılıyken kullanılamaz)
	void SetDocked(bool bNewDocked);
	bool IsDocked() const { return bDocked; }

	void AddCharge(float Amount);

	UFUNCTION(BlueprintPure, Category = "NL|Flashlight") float GetBatteryPercent() const { return Battery / 100.f; }
	UFUNCTION(BlueprintPure, Category = "NL|Flashlight") bool IsOn() const { return bOn; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "NL|Flashlight") float OnIntensity = 6000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Flashlight") float StartBattery = 100.f;
	// Saniyede düşen pil (100 üzerinden). 0.5 => ~3.3 dk kesintisiz ışık
	UPROPERTY(EditAnywhere, Category = "NL|Flashlight") float DrainPerSecond = 0.5f;
	// Bu yüzdenin altında titremeye başlar
	UPROPERTY(EditAnywhere, Category = "NL|Flashlight") float FlickerThreshold = 0.15f;

	// Açma sesi (OffSound boşsa kapamada da çalar)
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ToggleSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> OffSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> EmptySound;

private:
	float Battery = 100.f;
	bool bOn = false;
	bool bDocked = false;
	float FlickerTimer = 0.f;

	void ApplyLight();
};
