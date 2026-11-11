// NoLightAfterMidnight - Jeneratör: yakıt, arıza, SPACE ile tamir mini oyunu
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLTypes.h"
#include "NLGenerator.generated.h"

class UStaticMeshComponent;
class UAudioComponent;
class USoundBase;
class UAnimMontage;
class ANLPlayerCharacter;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLGenerator : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLGenerator();
	virtual void Tick(float DeltaSeconds) override;

	// INLInteractable
	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

	UFUNCTION(BlueprintPure, Category = "NL|Generator")
	bool IsProducingPower() const { return Fuel > 0.f && !bBroken; }

	UFUNCTION(BlueprintPure, Category = "NL|Generator")
	bool IsBroken() const { return bBroken; }

	UFUNCTION(BlueprintPure, Category = "NL|Generator")
	float GetFuelPercent() const { return MaxFuel > 0.f ? Fuel / MaxFuel : 0.f; }

	UFUNCTION(BlueprintCallable, Category = "NL|Generator")
	void AddFuel(float Amount);

	// Jeneratörü hemen arızalandırır (zamanlayıcı da bunu çağırır)
	UFUNCTION(BlueprintCallable, Category = "NL|Generator")
	void BreakDown();

	// --- Tamir mini oyunu ---
	bool IsBeingRepaired() const { return RepairingPlayer.IsValid(); }
	void StartRepair(ANLPlayerCharacter* Player);
	void CancelRepair();
	// Oyuncu SPACE'e bastığında çağrılır
	void HandleRepairInput();

	float GetRepairProgress() const { return RequiredRepairHits > 0 ? (float)RepairHits / RequiredRepairHits : 1.f; }
	float GetNeedlePosition() const { return NeedlePos; }
	float GetZoneStart() const { return ZoneStart; }
	float GetZoneWidth() const { return ZoneWidth; }

	UPROPERTY(BlueprintAssignable, Category = "NL|Generator")
	FNLOnSimpleEvent OnBrokeDown;

	UPROPERTY(BlueprintAssignable, Category = "NL|Generator")
	FNLOnSimpleEvent OnRepaired;

	UPROPERTY(BlueprintAssignable, Category = "NL|Generator")
	FNLOnSimpleEvent OnRepairMistake;

	UPROPERTY(BlueprintAssignable, Category = "NL|Generator")
	FNLOnSimpleEvent OnFuelEmpty;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NL|Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NL|Components")
	TObjectPtr<UAudioComponent> HumAudio;

	// ---- Yakıt ----
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "1"))
	float MaxFuel = 100.f;

	// Gün 1: %50 benzinle başlar
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "0"))
	float StartFuel = 50.f;

	// Saniyede düşen yakıt. 0.1 => %50 yakıt ~8 dakika
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "0"))
	float FuelDrainPerSecond = 0.1f;

	// ---- Arıza ----
	UPROPERTY(EditAnywhere, Category = "NL|Breakdown")
	bool bRandomBreakdowns = true;

	UPROPERTY(EditAnywhere, Category = "NL|Breakdown", meta = (ClampMin = "5"))
	float MinBreakdownInterval = 90.f;

	UPROPERTY(EditAnywhere, Category = "NL|Breakdown", meta = (ClampMin = "5"))
	float MaxBreakdownInterval = 180.f;

	// ---- Tamir ----
	// Kaç başarılı SPACE vuruşu gerekir
	UPROPERTY(EditAnywhere, Category = "NL|Repair", meta = (ClampMin = "1"))
	int32 RequiredRepairHits = 3;

	// İbre saniyede kaç "bar boyu" ilerler
	UPROPERTY(EditAnywhere, Category = "NL|Repair", meta = (ClampMin = "0.1"))
	float NeedleSpeed = 1.2f;

	// Doğru bölgenin genişliği (0-1)
	UPROPERTY(EditAnywhere, Category = "NL|Repair", meta = (ClampMin = "0.05", ClampMax = "0.9"))
	float ZoneWidth = 0.18f;

	// ---- Sesler ----
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> HumLoopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> EngineStartSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> EngineStopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> BreakdownSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> RepairHitSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> RepairMistakeSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> RepairCompleteSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> RefuelSound;

	// ---- Oyuncu animasyonları ----
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerRepairMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerRefuelMontage;

private:
	float Fuel = 0.f;
	bool bBroken = false;
	float BreakdownTimer = 0.f;

	TWeakObjectPtr<ANLPlayerCharacter> RepairingPlayer;
	int32 RepairHits = 0;
	float NeedlePos = 0.f;
	float NeedleDir = 1.f;
	float ZoneStart = 0.4f;

	void ScheduleNextBreakdown();
	void RandomizeZone();
	void NotifyPowerChanged(bool bPlayTransitionSounds);
	void FinishRepair();
};
