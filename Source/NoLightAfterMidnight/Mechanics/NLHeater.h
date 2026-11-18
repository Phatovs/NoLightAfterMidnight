// NoLightAfterMidnight - Eski elektrikli ısıtıcı (soba)
// Model: Scripts/blender_heater.py (SM_NL_Heater). Elektrik varken çubuklar yavaşça kızarır, turuncu ışık verir ve hafif uğuldar;
// elektrik gidince söner. Yakınında (WarmRadius, aynı odada) duran oyuncu ısınır (ANLPlayerCharacter soğuk mekaniği).
// Deniz feneri (ANLLighthouse) bekçi evinin içine oyun başında kendisi koyar; başka yere de elle yerleştirilebilir.
// Yerel eksenler: orijin = taban ortası, +X = ön yüz (ısıtıcı çubuklar).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLHeater.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UNLPowerConsumerComponent;
class USoundBase;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLHeater : public AActor
{
	GENERATED_BODY()

public:
	ANLHeater();
	virtual void Tick(float DeltaSeconds) override;

	// Bu noktada duran biri ısınıyor mu (açık, yakın ve arada duvar yok)
	bool WarmsLocation(const FVector& Location) const;
	bool IsRunning() const { return HeatAlpha > 0.5f; }
	// Dünyadaki herhangi bir ısıtıcı bu noktayı ısıtıyor mu
	static bool IsWarmAt(const UWorld* World, const FVector& Location);

	// Isıttığı mesafe (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Heater") float WarmRadius = 430.f;
	// Çubukların açılınca tam kızarma ve kapanınca sönme süresi (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Heater") float WarmUpTime = 2.5f;
	UPROPERTY(EditAnywhere, Category = "NL|Heater") float CoolDownTime = 5.f;
	UPROPERTY(EditAnywhere, Category = "NL|Heater") float GlowIntensity = 7.f;
	UPROPERTY(EditAnywhere, Category = "NL|Heater") float LightIntensity = 9.f;
	UPROPERTY(EditAnywhere, Category = "NL|Heater") FLinearColor LightColor = FLinearColor(1.f, 0.38f, 0.1f);
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> HumLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") float HumVolume = 0.18f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Hum;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNLPowerConsumerComponent> PowerConsumer;

private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	float HeatAlpha = 0.f;
	float FlickerTime = 0.f;
	static TArray<TWeakObjectPtr<ANLHeater>> AllHeaters;
	void Apply();
};
