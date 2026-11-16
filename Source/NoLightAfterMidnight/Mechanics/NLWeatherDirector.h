// NoLightAfterMidnight - Hava durumu: yağmurlu geceler (varsayılan: sadece 1. gece)
// - Oyuncunun çevresinde düşen yağmur damlaları. Her damla doğduğu yerde yukarıdan aşağı iz düşürür,
//   çatıya/zemine değince yeniden doğar: binaların içine yağmur girmez, kapıdan/camdan dışarıda yağdığı görülür.
// - Damlalar ışıktan etkilenir: fenerle bakınca parlar.
// - Ses: dışarıda yağmur, içeride çatıya vuran boğuk yağmur. Ara ara şimşek (gökyüzü aydınlanır) ve gök gürültüsü.
// - Yağmurlu gecede sis biraz koyulaşır.
// GameMode, level'de yoksa kendisi oluşturur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLWeatherDirector.generated.h"

class UAudioComponent;
class USoundBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLWeatherDirector : public AActor
{
	GENERATED_BODY()

public:
	ANLWeatherDirector();
	virtual void Tick(float DeltaSeconds) override;

	// Yağmur yağacak geceler (1 = ilk gece)
	UPROPERTY(EditAnywhere, Category = "NL|Weather") TArray<int32> RainyDays = { 1 };
	// Yağmurun başlaması / dinmesi (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Weather") float RainFadeTime = 6.f;

	// --- Görüntü ---
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") int32 DropCount = 5000;
	// Oyuncunun çevresinde yağmurun yağdığı alan: yatay yarıçap ve yükseklik (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float AreaRadius = 1000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float AreaHeight = 1400.f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float FallSpeed = 1500.f;
	// Rüzgâr: damlalar hafif eğik düşer (cm/sn)
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") FVector Wind = FVector(260.f, 90.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float DropLength = 85.f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float DropWidth = 0.32f;
	// Damlanın saydamlığı ve ışıksız parlaklığı (fener ışığı ayrıca parlatır)
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float DropOpacity = 0.3f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float DropGlow = 0.05f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") TObjectPtr<UMaterialInterface> RainMaterial;
	// Yağmurda sis bu kadar koyulaşır (çarpan)
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Visual") float FogDensityMultiplier = 1.6f;

	// --- Ses ---
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Audio") TObjectPtr<USoundBase> RainLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Audio") TObjectPtr<USoundBase> RainRoofLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Audio") TArray<TObjectPtr<USoundBase>> ThunderSounds;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Audio") float RainVolume = 0.75f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Audio") float RainRoofVolume = 0.6f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Audio") float ThunderVolume = 1.f;

	// --- Şimşek ---
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Lightning") bool bLightning = true;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Lightning") float FirstLightningDelay = 25.f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Lightning") float LightningGapMin = 35.f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Lightning") float LightningGapMax = 80.f;
	UPROPERTY(EditAnywhere, Category = "NL|Weather|Lightning") float LightningIntensity = 4.f;

	bool IsRaining() const { return bRaining; }
	// Test: yağmuru elle aç/kapat, hemen şimşek çaktır
	void SetRaining(bool bOn, bool bInstant = false);
	void TriggerLightning();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Drops;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DropMID;
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> Flash;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RainAudio;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RoofAudio;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> Fog;

	struct FDrop
	{
		FVector Pos;
		float Speed;
		float KillZ;
	};
	TArray<FDrop> DropData;
	TArray<FTransform> DropXforms;

	bool bRaining = false;
	float Intensity = 0.f;      // 0..1, yağmurun şiddeti (açılıp kapanırken)
	float BaseFogDensity = -1.f;
	float LightningTimer = 0.f;
	float FlashTime = -1.f;     // şimşek başladığından beri geçen süre (<0: yok)
	TArray<float> FlashPulses;  // şimşeğin titreme anları
	float PendingThunderTime = -1.f;
	float PendingThunderVol = 1.f;
	int32 LastThunder = INDEX_NONE;
	float IndoorAlpha = 0.f;

	UFUNCTION() void HandleDayStarted(int32 Day);
	void RespawnDrop(FDrop& D, const FVector& Center, bool bAnyHeight);
	void UpdateDrops(float DeltaSeconds, const FVector& Center);
	void UpdateLightning(float DeltaSeconds);
	void UpdateAudio(float DeltaSeconds);
};
