// NoLightAfterMidnight - Drowned (Boğulmuş): görsel/animasyon/ses tarafı
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLJumpscareGrab.h"
#include "NLDrowned.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class USoundBase;
class UAudioComponent;
class UNLBodyRigComponent;

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
	virtual void Tick(float DeltaSeconds) override;

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
	// Görünen gövde: montaj yokken pozlar (yüz siperi, kaçış, saldırı) kodla verilir. GetMesh() görünmez sürücüdür.
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLBodyRigComponent> BodyRig;

	// Işık yiyince geriye eğilme (derece) ve kaçarken uzaklaşma mesafesi (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FlinchLeanBack = 22.f;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FleeDistance = 320.f;
	// Kaçarken oynayan koşu animasyonu (montaj yoksa). Arkasında duvar varsa mesafe kısalır.
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimSequenceBase> FleeRunAnimation;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FleeRunPlayRate = 1.15f;
	// Jumpscare: yakaladıktan sonra yüzün kameraya ne kadar daha yaklaştığı (cm) ve bakışın sarsılma şiddeti (derece)
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareLunge = 16.f;
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareCameraShake = 1.6f;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> AppearMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> CoverFaceMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> FleeMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> JumpscareMontage;

	// Montaj atanmamışsa kullanılacak süreler
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackCoverFaceDuration = 1.2f;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackFleeDuration = 1.4f;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackJumpscareDuration = 1.5f;

	// Jumpscare'de yüz kameranın kaç cm önünde olur
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareDistance = 55.f;
	// Yüzü kameranın tam karşısına getirmek için kullanılan kemik (UE mankeninde "head")
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") FName HeadBoneName = TEXT("head");
	// Kafa kemiği kafatasının dibindedir; yüzün ortası ekranın ortasına gelsin diye kemik bu kadar aşağıda tutulur
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float FaceCenterOffset = 10.f;
	// Kemik bulunamazsa kapsül merkezinden kafaya olan yükseklik
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float FallbackHeadHeight = 75.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> BreathLoopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> AppearSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ScreechSound;   // ışık yiyince
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> JumpscareSound;

private:
	bool bShown = false;

	// Kodla verilen pozlar (montaj atanmamışsa)
	enum class EPose : uint8 { None, CoverFace, Flee, Jumpscare };
	EPose Pose = EPose::None;
	float PoseTimer = 0.f;
	float PoseDuration = 1.f;
	FVector FleeStart = FVector::ZeroVector;
	FRotator FleeStartRot = FRotator::ZeroRotator;
	float FleeMaxDistance = 0.f;
	bool bRunAnimPlaying = false;
	void StopRunAnimation();
	FNLJumpscareGrab Grab;
	void StartPose(EPose NewPose, float Duration);
	void UpdatePose(float DeltaSeconds);
	float PlayMontageSafe(UAnimMontage* Montage, float Fallback);
};
