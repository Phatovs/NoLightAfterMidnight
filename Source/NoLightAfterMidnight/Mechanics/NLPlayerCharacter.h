// NoLightAfterMidnight - Liam: birinci şahıs karakter
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLTypes.h"
#include "NLPlayerCharacter.generated.h"

class UCameraComponent;
class UNLFlashlightComponent;
class UInputAction;
class UInputMappingContext;
class UAnimMontage;
class USoundBase;
class UNLHUDWidget;
class ANLSecurityDesk;
class ANLGenerator;
struct FInputActionValue;
struct FInputActionInstance;

/**
 * Kontroller (Enhanced Input, C++ içinde oluşturulur - editörde asset gerekmez):
 *  WASD yürü, Fare bak, E etkileşim/kalk, SPACE ışık (masada) veya tamir (jeneratörde),
 *  F fener, masadayken A/D veya Sol/Sağ ok kamera değiştirir, 1-8 kamerayı doğrudan seçer.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANLPlayerCharacter();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	// --- Masa ---
	void SitAtDesk(ANLSecurityDesk* Desk);
	void StandUp();

	// --- Tamir ---
	void EnterRepairMode(ANLGenerator* Generator, UAnimMontage* RepairMontage);
	void ExitRepairMode();

	// --- Benzin ---
	bool HasFuelCan() const { return bHasFuelCan; }
	void PickupFuelCan(float Amount);
	float ConsumeFuelCan();

	// --- Ölüm ---
	void Die();
	void LookAtKiller(AActor* Killer);

	void PlayBodyMontage(UAnimMontage* Montage);

	UFUNCTION(BlueprintPure, Category = "NL|Player") ENLPlayerMode GetMode() const { return Mode; }
	ANLSecurityDesk* GetCurrentDesk() const { return CurrentDesk.Get(); }
	ANLGenerator* GetCurrentGenerator() const { return CurrentGenerator.Get(); }
	UNLFlashlightComponent* GetFlashlight() const { return Flashlight; }
	FText GetCurrentInteractPrompt() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLFlashlightComponent> Flashlight;

	UPROPERTY(EditAnywhere, Category = "NL|Player") float InteractDistance = 250.f;
	UPROPERTY(EditAnywhere, Category = "NL|Player") float WalkSpeed = 280.f;
	UPROPERTY(EditAnywhere, Category = "NL|Player") float MouseSensitivity = 0.6f;

	UPROPERTY(EditAnywhere, Category = "NL|UI") TSubclassOf<UNLHUDWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> SitMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> StandMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> FootstepSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") float FootstepInterval = 0.55f;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> StandUpSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> DeathSound;

private:
	ENLPlayerMode Mode = ENLPlayerMode::Walking;
	TWeakObjectPtr<ANLSecurityDesk> CurrentDesk;
	TWeakObjectPtr<ANLGenerator> CurrentGenerator;
	TWeakObjectPtr<AActor> FocusedActor;
	bool bHasFuelCan = false;
	float CarriedFuel = 0.f;
	float FootstepTimer = 0.f;

	UPROPERTY(Transient) TObjectPtr<UNLHUDWidget> HUDWidget;

	// Çalışma anında oluşturulan input asset'leri
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SpaceAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FlashlightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CameraNextAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CameraPrevAction;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> CameraSelectActions;

	void BuildInput();
	UInputAction* MakeAction(const TCHAR* Name, bool bAxis2D);

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnInteract();
	void OnSpace();
	void OnFlashlight();
	void OnCameraNext();
	void OnCameraPrev();
	void OnCameraSelect(const FInputActionInstance& Instance);

	void UpdateFocus();
	void UpdateFootsteps(float DeltaSeconds);
	void CreateHUD();
};
