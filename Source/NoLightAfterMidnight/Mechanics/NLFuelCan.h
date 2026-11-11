// NoLightAfterMidnight - Kulübedeki benzin bidonu
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLFuelCan.generated.h"

class UStaticMeshComponent;
class USoundBase;
class UAnimMontage;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLFuelCan : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLFuelCan();

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;
	virtual bool CanInteract(const ANLPlayerCharacter* Player) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> Mesh;

	// Bidon jeneratöre ne kadar yakıt ekler (MaxFuel = 100 üzerinden)
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "1")) float FuelAmount = 40.f;

	// Alındıktan kaç saniye sonra kulübede yeni bidon belirir (0 = bir daha gelmez)
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "0")) float RespawnDelay = 60.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> PickupSound;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerPickupMontage;

private:
	bool bAvailable = true;
	FTimerHandle RespawnTimer;
	void SetAvailable(bool bNewAvailable);
	void Respawn();
};
