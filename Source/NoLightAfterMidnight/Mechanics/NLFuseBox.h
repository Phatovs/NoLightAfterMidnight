// NoLightAfterMidnight - Fuse: ana güç anahtarı
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLTypes.h"
#include "NLFuseBox.generated.h"

class UStaticMeshComponent;
class USoundBase;
class UAnimMontage;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLFuseBox : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLFuseBox();
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

	UFUNCTION(BlueprintPure, Category = "NL|Fuse")
	bool IsOn() const { return bOn; }

	// Kendiliğinden attı mı (oyuncu kapatmadı)
	UFUNCTION(BlueprintPure, Category = "NL|Fuse")
	bool IsTripped() const { return bTripped; }

	// Test için: şalter bu kadar saniye sonra atsın
	void SetTripTimer(float Seconds) { TripTimer = Seconds; }

	UFUNCTION(BlueprintCallable, Category = "NL|Fuse")
	void SetOn(bool bNewOn);

	// Fuse atar (ileride sabotaj / aşırı yük için)
	UFUNCTION(BlueprintCallable, Category = "NL|Fuse")
	void Trip();

	UPROPERTY(BlueprintAssignable, Category = "NL|Fuse")
	FNLOnPowerChanged OnSwitched;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> BoxMesh;
	// Şalter kolu, açık/kapalı rotasyonları arasında döner
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> LeverMesh;

	UPROPERTY(EditAnywhere, Category = "NL|Fuse") bool bStartOn = true;
	// Şalter zaman zaman kendiliğinden atar (şema: "Fuse arızalanırsa tüm elektrikler gider"). Sadece gün sürerken ve açıkken sayar.
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") bool bRandomTrips = true;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse", meta = (ClampMin = "5")) float MinTripInterval = 120.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse", meta = (ClampMin = "5")) float MaxTripInterval = 240.f;
	// Şalter kolu: kutunun ön yüzündeki mafsaldan dışarı çıkar. Açıkken yukarı, kapalıyken aşağı bakar.
	// LeverPivot ve LeverOutward kutunun (BoxMesh) kendi koordinatlarında.
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FVector LeverPivot = FVector(50.f, 100.f, 50.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FVector LeverOutward = FVector(0.f, 1.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverLength = 30.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverOnAngle = 45.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverOffAngle = -45.f;
	// Kolun dönüş hızı (derece/sn)
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverSpeed = 400.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SwitchOnSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SwitchOffSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> TripSound;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerSwitchMontage;
	// Şalteri çekme hareketinin süresi (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Animation") float SwitchActionDuration = 1.1f;

private:
	bool bOn = true;
	bool bTripped = false;
	float LeverAngle = 0.f;
	void PlaceLever(float AngleDeg);
	float TripTimer = 0.f;
	void ScheduleNextTrip();
	void ApplyVisuals();
};
