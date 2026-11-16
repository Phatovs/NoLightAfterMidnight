// NoLightAfterMidnight - Ana menü sahnesi
// Karanlık kulübenin içi. Kamera sabit; sağ altta elde tutulan bir el feneri fare imlecinin gösterdiği yeri aydınlatır
// (menü yazıları da fenerin vurduğu yerde parlar, bkz. UNLMainMenuWidget). Dışarıda yağmur, ara ara camdan şimşek çakar.
// NLMenuGameMode ile kullanılır; level'e bir tane konur (Scripts/nl_make_menu.py).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "NLMenuDirector.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class UNLMainMenuWidget;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLMenuDirector : public AActor
{
	GENERATED_BODY()

public:
	ANLMenuDirector();
	virtual void Tick(float DeltaSeconds) override;

	// Elde tutulan el feneri (iç mekân menüsü). Kapalıysa level'deki deniz fenerinin huzmesi imleci izler.
	UPROPERTY(EditAnywhere, Category = "NL|Menu") bool bHandFlashlight = true;
	// İç mekân: çatıya vuran yağmur + kendi şimşeği. Dış mekân: dalga + rüzgâr (yağmur/şimşek NLWeatherDirector'dan)
	UPROPERTY(EditAnywhere, Category = "NL|Menu") bool bIndoor = true;
	// Deniz feneri huzmesi kameradan imleç yönünde bu uzaklıktaki noktaya döner
	UPROPERTY(EditAnywhere, Category = "NL|Menu") float LighthouseAimDistance = 1600.f;
	UPROPERTY(EditAnywhere, Category = "NL|Menu") float FlashlightIntensity = 5000.f;
	// Fenerin imleci takip hızı (düşük = elde ağır ağır döner)
	UPROPERTY(EditAnywhere, Category = "NL|Menu") float AimSpeed = 7.f;
	// Şimşek ışığı (pencerenin dışı, dünya konumu)
	UPROPERTY(EditAnywhere, Category = "NL|Menu") FVector LightningLocation = FVector(140.f, -600.f, 300.f);
	UPROPERTY(EditAnywhere, Category = "NL|Menu") float LightningGapMin = 9.f;
	UPROPERTY(EditAnywhere, Category = "NL|Menu") float LightningGapMax = 22.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> FlashPivot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlashBody;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FlashHead;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Flashlight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Lightning;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RainAudio;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> RoofAudio;
	UPROPERTY(Transient) TWeakObjectPtr<class ANLLighthouse> Lighthouse;
	UPROPERTY(Transient) TObjectPtr<UNLMainMenuWidget> Menu;
public:
	UNLMainMenuWidget* GetMenu() const { return Menu; }
protected:

	UPROPERTY(EditAnywhere, Category = "NL|Menu") TObjectPtr<USoundBase> RainLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Menu") TObjectPtr<USoundBase> RoofLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Menu") TArray<TObjectPtr<USoundBase>> Thunder;
	UPROPERTY(EditAnywhere, Category = "NL|Menu") TObjectPtr<USoundBase> WavesLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Menu") TObjectPtr<USoundBase> WindLoop;

private:
	float LightningTimer = 6.f;
	float FlashT = -1.f;
	float ThunderIn = -1.f;
	float Time = 0.f;
	FRotator AimRot = FRotator::ZeroRotator;
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANLMenuGameMode();

	// Test: imleci menü öğelerinin üstüne götürüp ekran görüntüsü alır, ayarları açar, çıkar
	UFUNCTION(Exec) void NLDebugMenuShots();
};
