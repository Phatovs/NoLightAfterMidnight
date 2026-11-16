// NoLightAfterMidnight - Ses atmosferi
// - Rüzgâr şiddeti dalgalanır (ara ara sertleşir), kıyıda konumlu dalga sesleri
// - Uzaktan, oyuncunun arkasından/yanından tekil sesler (dal kırılması, fısıltı, metal, adım...) rastgele aralıklarla
// - Bina içine girince dış sesler boğuklaşıp kısılır, alçak bir oda uğultusu gelir
// GameMode, level'de yoksa kendisi oluşturur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLAmbienceDirector.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLAmbienceDirector : public AActor
{
	GENERATED_BODY()

public:
	ANLAmbienceDirector();
	virtual void Tick(float DeltaSeconds) override;

	// --- Katmanlar ---
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") TObjectPtr<USoundBase> GustLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") TObjectPtr<USoundBase> WavesLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") TObjectPtr<USoundBase> InteriorLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float GustVolume = 0.55f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float WavesVolume = 0.8f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float InteriorVolume = 0.5f;
	// Kıyı: adanın merkezinden dışarı ışınlar, zeminin bu yüksekliğin altına indiği yer kıyıdır
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float SeaLevelZ = 20.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") int32 ShoreRays = 12;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float ShoreSearchRadius = 7000.f;

	// --- Uzaktan tekil sesler ---
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") TArray<TObjectPtr<USoundBase>> DistantSounds;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float FirstDistantDelay = 20.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float DistantGapMin = 18.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float DistantGapMax = 45.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float DistantMinDistance = 1500.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float DistantMaxDistance = 4000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float DistantVolume = 0.9f;

	// --- İç mekân ---
	// Oyuncunun üstünde bu mesafede tavan varsa "içeride" sayılır
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float RoofCheckHeight = 1500.f;
	// İçerideyken dış seslerin sesi ve boğukluğu (alçak geçiren filtre, Hz)
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float IndoorOutsideVolume = 0.25f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float IndoorLowPassHz = 400.f;

	// --- Camlı bina (deniz feneri): "NLWindowGlass" etiketli aktörün içindeyken camlara çarpan rüzgâr ---
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") FName GlassTag = TEXT("NLWindowGlass");
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") TObjectPtr<USoundBase> GlassWindLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") TObjectPtr<USoundBase> GlassGustHit;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float GlassWindVolume = 0.7f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float GlassHitVolume = 0.8f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float GlassHitGapMin = 3.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambience") float GlassHitGapMax = 9.f;

	// Test: birkaç saniyede bir tekil ses çal
	void DebugBurst(int32 Count, float Gap);
	float GetIndoorAlpha() const { return IndoorAlpha; }
	float GetGlassAlpha() const { return GlassAlpha; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> GustAudio;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> InteriorAudio;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> GlassAudio;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> GlassActors;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> GlassAttenuation;
	UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> WaveAudios;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> DistantAttenuation;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> WavesAttenuation;

	float IndoorAlpha = 0.f;
	bool bIndoor = false;
	float IndoorCheckTimer = 0.f;
	float DistantTimer = 0.f;
	float GustPhase = 0.f;
	int32 LastDistantIndex = INDEX_NONE;
	float GlassAlpha = 0.f;
	float GlassHitTimer = 2.f;
	const AActor* CurrentGlass = nullptr;
	void UpdateGlass(float DeltaSeconds, const class ANLPlayerCharacter* Player);

	void SetupShore();
	void PlayDistantSound();
	bool IsQuietMoment() const;
	void ApplyMix();
};
