// NoLightAfterMidnight - Drowned (Boğulmuş): görsel/animasyon/ses tarafı
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLDrowned.generated.h"

class UAnimMontage;
class USoundBase;
class UAudioComponent;

/**
 * Drowned'ın kendisi sadece "oyuncu"dur: belirir, elini yüzüne kapatır, kaçar, saldırır.
 * Ne zaman ne yapacağına ANLDrownedDirector karar verir.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLDrowned : public ACharacter
{
	GENERATED_BODY()

public:
	ANLDrowned();

	// Kameranın spawn noktasında belirir
	void AppearAt(const FTransform& SpawnTransform);

	// Işık yedi: elini yüzüne kapatır. Süreyi döndürür.
	float PlayCoverFace();

	// Kaçış animasyonu. Süreyi döndürür.
	float PlayFlee();

	// Gizlenir (başka kameraya geçerken / haritadan ayrılırken)
	void Vanish();

	// Işık tutulmadı: oyuncunun kamerasının önüne ışınlanıp saldırır
	float PerformJumpscare(APlayerController* Victim);

	bool IsShown() const { return bShown; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UAudioComponent> BreathAudio;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> AppearMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> CoverFaceMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> FleeMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> JumpscareMontage;

	// Montaj atanmamışsa kullanılacak süreler
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackCoverFaceDuration = 1.2f;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackFleeDuration = 0.8f;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackJumpscareDuration = 1.5f;

	// Jumpscare'de kameranın kaç cm önünde belirir
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareDistance = 70.f;
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareHeightOffset = -150.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> BreathLoopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> AppearSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ScreechSound;   // ışık yiyince
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> JumpscareSound;

private:
	bool bShown = false;
	float PlayMontageSafe(UAnimMontage* Montage, float Fallback);
};
