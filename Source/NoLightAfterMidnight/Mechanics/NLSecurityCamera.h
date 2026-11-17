// NoLightAfterMidnight - Güvenlik kamerası + altındaki savunma ışığı
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLTypes.h"
#include "NLSecurityCamera.generated.h"

class USceneCaptureComponent2D;
class USpotLightComponent;
class UStaticMeshComponent;
class UArrowComponent;
class UTextureRenderTarget2D;
class UNLPowerConsumerComponent;
class USoundBase;

/**
 * Haritadaki 8 kameradan biri.
 * - SceneCapture: sadece aktif kamera görüntü çizer (performans).
 * - DefenseLight: SPACE ile yakılan güçlü parlama + yavaş sönme, 2 sn cooldown.
 * - DrownedSpawnPoint: Drowned'ın bu kamerada belireceği yer (ışık menzilinde).
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLSecurityCamera : public AActor
{
	GENERATED_BODY()

public:
	ANLSecurityCamera();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "NL|Camera")
	FText GetCameraName() const { return CameraName; }

	// Ekranda "CAM 1 KİLER" gibi görünen yer adı. Boşsa CameraName kullanılır.
	UFUNCTION(BlueprintPure, Category = "NL|Camera")
	FText GetLocationLabel() const { return LocationLabel; }

	// Masa bu kamerayı monitöre bağlar/çözer
	void SetFeedActive(bool bActive, UTextureRenderTarget2D* Target, UPrimitiveComponent* Screen = nullptr);

	// Işığı yakar. Elektrik yoksa veya cooldown sürüyorsa false döner.
	UFUNCTION(BlueprintCallable, Category = "NL|Camera")
	bool Flash();

	UFUNCTION(BlueprintPure, Category = "NL|Camera")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "NL|Camera")
	float GetCooldownPercent() const { return FlashCooldown > 0.f ? CooldownRemaining / FlashCooldown : 0.f; }

	FTransform GetDrownedSpawnTransform() const;

	UPROPERTY(BlueprintAssignable, Category = "NL|Camera")
	FNLOnCameraFlashed OnFlashed;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> CameraMesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneCaptureComponent2D> SceneCapture;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USpotLightComponent> DefenseLight;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UArrowComponent> DrownedSpawnPoint;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLPowerConsumerComponent> PowerConsumer;

	UPROPERTY(EditAnywhere, Category = "NL|Camera") FText CameraName = FText::FromString(TEXT("Cam01"));
	UPROPERTY(EditAnywhere, Category = "NL|Camera") FText LocationLabel;

	// --- CCTV görünümü (gece görüşü: gri, kontrastlı, grenli) ---
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") bool bApplyCCTVLook = true;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "0", ClampMax = "1")) float CCTVSaturation = 0.f;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "0", ClampMax = "3")) float CCTVContrast = 1.3f;
	// Karanlık sahnede görüntüyü aydınlatır (gece görüşü hissi)
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "-5", ClampMax = "8")) float CCTVExposureBias = 1.f;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") FLinearColor CCTVTint = FLinearColor(0.95f, 1.f, 0.98f);
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "0", ClampMax = "1")) float CCTVFilmGrain = 0.4f;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "0", ClampMax = "1")) float CCTVVignette = 0.7f;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "0", ClampMax = "8")) float CCTVBloom = 1.5f;
	// Kenarlarda renk kayması (lens hatası)
	UPROPERTY(EditAnywhere, Category = "NL|CCTV", meta = (ClampMin = "0", ClampMax = "5")) float CCTVChromaticAberration = 0.8f;

	UPROPERTY(EditAnywhere, Category = "NL|Light") float FlashPeakIntensity = 80000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Light") float FlashRiseTime = 0.06f;
	UPROPERTY(EditAnywhere, Category = "NL|Light") float FlashFadeTime = 1.6f;
	UPROPERTY(EditAnywhere, Category = "NL|Light") float FlashCooldown = 2.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> FlashSound;
	// Elektrik yokken / cooldown'dayken SPACE'e basılırsa
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> FlashFailSound;

	UFUNCTION()
	void HandlePowerChanged(bool bHasPower);

private:
	void ApplyCCTVLook();

	float FlashElapsed = -1.f;
	float CooldownRemaining = 0.f;
	bool bFeedActive = false;
	// Görüntünün gösterildiği monitör: ekranda değilse kamera çizmez (performans)
	TWeakObjectPtr<UPrimitiveComponent> FeedScreen;
	float CaptureCooldown = 0.f;
	void TickFeed(float DeltaSeconds);
};
