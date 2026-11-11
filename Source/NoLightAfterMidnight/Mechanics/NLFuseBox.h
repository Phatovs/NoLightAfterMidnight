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

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

	UFUNCTION(BlueprintPure, Category = "NL|Fuse")
	bool IsOn() const { return bOn; }

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
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FRotator LeverOnRotation = FRotator(30.f, 0.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FRotator LeverOffRotation = FRotator(-30.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SwitchOnSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SwitchOffSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> TripSound;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerSwitchMontage;

private:
	bool bOn = true;
	void ApplyVisuals();
};
