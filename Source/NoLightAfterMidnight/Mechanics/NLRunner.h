// NoLightAfterMidnight - Runner (Koşucu): tamir hatasında oyuncuyu kovalar
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLJumpscareGrab.h"
#include "NLRunner.generated.h"

class USphereComponent;
class UPointLightComponent;
class UAudioComponent;
class UAnimMontage;
class USoundBase;
class ANLPlayerCharacter;
class UNLBodyRigComponent;
class ANLHideDoor;

/**
 * Sahneye yerleştirin; konumu "geldiği yer" olarak kaydedilir ve gizli bekler.
 * Jeneratör tamirinde hata yapılınca GameMode onu çağırır -> NavMesh üzerinde oyuncuyu kovalar.
 * KillSphere oyuncuya değerse ölüm. Kovalama ancak oyuncu bir odaya girip kapıyı kapatınca biter (ANLHideDoor).
 * Seviyede bir NavMeshBoundsVolume olmalı!
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLRunner : public ACharacter
{
	GENERATED_BODY()

public:
	ANLRunner();
	virtual void Tick(float DeltaSeconds) override;

	// Tamir hatası: Runner uzakta (görünmeyen bir yerde) belirir, uzaktan çığlık atar ve oyuncu tamirden çıkınca koşmaya başlar
	void PrepareChase(ANLPlayerCharacter* Target);
	// Pusu (rastgele olay): uzakta belirir, çığlık atar (oyuncunun her yerden duyacağı şekilde), Delay saniye sonra kovalamaya başlar
	void PrepareAmbush(ANLPlayerCharacter* Target, float Delay, float MinDistance, float MaxDistance);
	// Hemen kovala (From: başlangıç noktası)
	void StartChase(ANLPlayerCharacter* Target, const FTransform& From);
	void StopChase();

	// Saklanma kapısı: oyuncu odaya kapandı -> kapının dışına geç ve zorla
	void BeginDoorAssault(ANLHideDoor* Door);
	// Kapı tutuldu: bırakıp gider
	void LeaveDoor();
	// Kapı tutulamadı: kapıyı açıp oyuncuyu yakalar (jumpscare)
	void DoorKill(ANLPlayerCharacter* Target);

	UFUNCTION(BlueprintPure, Category = "NL|Runner") bool IsChasing() const { return bChasing || bPendingChase || bDoorAssault; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USphereComponent> KillSphere;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UAudioComponent> ChaseAudio;
	// Karanlık odada kapıdan dalınca yüzü alttan aydınlatan soluk ışık (sadece kapı jumpscare'inde)
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UPointLightComponent> FaceLight;
	// Görünen gövde (GetMesh() görünmez sürücü: koşu animasyonu). Yakalama jumpscare pozu kodla verilir.
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLBodyRigComponent> BodyRig;

	// Yakalama jumpscare ayarları (Boğulmuş ile aynı hareket)
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareDistance = 55.f;
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareLunge = 16.f;
	UPROPERTY(EditAnywhere, Category = "NL|Jumpscare") float JumpscareCameraShake = 1.6f;

	UPROPERTY(EditAnywhere, Category = "NL|Runner") float ChaseSpeed = 250.f;   // oyuncunun yürüme hızının (280) biraz altında: kaçılabilir
	// 0 = süresiz: oyuncu bir odaya kapanana (ya da yakalanana) kadar kovalar
	UPROPERTY(EditAnywhere, Category = "NL|Runner") float ChaseDuration = 0.f;
	// Tamir hatasında oyuncudan bu aralıkta (cm), tercihen görmediği bir yerde belirir
	UPROPERTY(EditAnywhere, Category = "NL|Runner") float SpawnMinDistance = 1200.f;
	UPROPERTY(EditAnywhere, Category = "NL|Runner") float SpawnMaxDistance = 2600.f;
	UPROPERTY(EditAnywhere, Category = "NL|Runner") float RepathInterval = 0.25f;
	UPROPERTY(EditAnywhere, Category = "NL|Runner") float KillRadius = 80.f;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> SpawnMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> KillMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float FallbackKillDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ChaseStartScream;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ChaseLoopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> KillSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> RetreatSound;

	UFUNCTION()
	void OnKillSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bChasing = false;
	bool bPendingChase = false;
	bool bDoorAssault = false;
	FTransform PendingSpawn;
	FTransform FindSpawnPoint(const ANLPlayerCharacter* Target) const;
	FTransform FindSpawnPoint(const ANLPlayerCharacter* Target, float MinDistance, float MaxDistance) const;
	float PendingDelay = 0.f;   // pusu: çığlıktan sonra koşmaya başlamadan önceki bekleme
	float ChaseTimer = 0.f;
	float RepathTimer = 0.f;
	FTransform HomeTransform;
	TWeakObjectPtr<ANLPlayerCharacter> ChaseTarget;
	FNLJumpscareGrab Grab;
	float GrabTimer = 0.f;
	float GrabDuration = 1.5f;

	void SetDormant(bool bDormant);
	void KillTarget();
};
