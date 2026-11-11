// NoLightAfterMidnight - Runner (Koşucu): tamir hatasında oyuncuyu kovalar
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLRunner.generated.h"

class USphereComponent;
class UAudioComponent;
class UAnimMontage;
class USoundBase;
class ANLPlayerCharacter;

/**
 * Sahneye yerleştirin; konumu "geldiği yer" olarak kaydedilir ve gizli bekler.
 * Jeneratör tamirinde hata yapılınca GameMode onu çağırır -> NavMesh üzerinde oyuncuyu kovalar.
 * KillSphere oyuncuya değerse ölüm. ChaseDuration dolarsa geri çekilir.
 * Seviyede bir NavMeshBoundsVolume olmalı!
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLRunner : public ACharacter
{
	GENERATED_BODY()

public:
	ANLRunner();
	virtual void Tick(float DeltaSeconds) override;

	void StartChase(ANLPlayerCharacter* Target);
	void StopChase();

	UFUNCTION(BlueprintPure, Category = "NL|Runner") bool IsChasing() const { return bChasing; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USphereComponent> KillSphere;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UAudioComponent> ChaseAudio;

	UPROPERTY(EditAnywhere, Category = "NL|Runner") float ChaseSpeed = 620.f;
	UPROPERTY(EditAnywhere, Category = "NL|Runner") float ChaseDuration = 20.f;
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
	float ChaseTimer = 0.f;
	float RepathTimer = 0.f;
	FTransform HomeTransform;
	TWeakObjectPtr<ANLPlayerCharacter> ChaseTarget;

	void SetDormant(bool bDormant);
	void KillTarget();
};
