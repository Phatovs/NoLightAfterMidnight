// NoLightAfterMidnight - Fuse kutusunun içindeki fener şarj cihazı
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLFlashlightCharger.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UNLPowerConsumerComponent;
class UNLFlashlightComponent;
class USoundBase;

/**
 * E: feneri şarja tak / geri al. Takılıyken fener kullanılamaz (gerilim!).
 * Sadece elektrik varken şarj eder. Gösterge ışığı: kırmızı = şarj oluyor, yeşil = dolu.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLFlashlightCharger : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLFlashlightCharger();
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UPointLightComponent> IndicatorLight;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLPowerConsumerComponent> PowerConsumer;

	// Saniyede eklenen pil (100 üzerinden)
	UPROPERTY(EditAnywhere, Category = "NL|Charger") float ChargePerSecond = 6.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> DockSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> UndockSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> FullSound;

private:
	TWeakObjectPtr<UNLFlashlightComponent> DockedFlashlight;
	bool bWasFull = false;
	void UpdateIndicator();
};
