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

	// Oyuncu bidonu yere bırakır (G). Bu bidon yeniden kulübede belirmişse yerine yeni bir bidon konur.
	void DropAt(const FVector& GroundLocation, float Yaw, float Amount);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> Mesh;

	// Bidon jeneratöre ne kadar yakıt ekler (MaxFuel = 100 üzerinden)
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "1")) float FuelAmount = 40.f;

	// Alındıktan kaç saniye sonra kulübede yeni bidon belirir (0 = bir daha gelmez)
	UPROPERTY(EditAnywhere, Category = "NL|Fuel", meta = (ClampMin = "0")) float RespawnDelay = 60.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> PickupSound;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerPickupMontage;

private:
	bool bAvailable = true;
	FTransform HomeTransform;
	float DefaultFuelAmount = 40.f;
	bool bDroppedCopy = false;
	FTimerHandle RespawnTimer;
	void SetAvailable(bool bNewAvailable);
	void Respawn();
};
