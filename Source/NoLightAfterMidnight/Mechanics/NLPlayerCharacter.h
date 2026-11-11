// NoLightAfterMidnight - Liam: birinci şahıs karakter
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLTypes.h"
#include "NLPlayerCharacter.generated.h"

class UCameraComponent;
class UNLFlashlightComponent;
class UNLBodyRigComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class UAnimMontage;
class USoundBase;
class UNLHUDWidget;
class ANLSecurityDesk;
class ANLGenerator;
struct FInputActionValue;
struct FInputActionInstance;

// Oyuncunun kısa el hareketleri (eli hedefe uzatır, doğru anda asıl işi yapar)
enum class ENLHandAction : uint8
{
	None,
	Switch,          // şalteri indir/kaldır
	DockFlashlight,  // feneri şarja koy
	TakeFlashlight,  // feneri şarjdan al
	Refuel           // bidonu kaldırıp jeneratöre dök
};

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
	// CanTop: bidonun dünyadaki tutma noktası (oyuncu eğilip oradan alır)
	void PickupFuelCan(float Amount, const FVector& CanTop);
	float ConsumeFuelCan();

	// --- Ölüm ---
	void Die();
	void LookAtKiller(AActor* Killer);

	void PlayBodyMontage(UAnimMontage* Montage);

	UFUNCTION(BlueprintPure, Category = "NL|Player") ENLPlayerMode GetMode() const { return Mode; }
	// Oturma hareketi bitti, monitöre bakıyor (kamera arayüzü ancak o zaman açılır)
	bool IsAtMonitor() const { return Mode == ENLPlayerMode::Seated && SitPhase == ESitPhase::Seated; }
	// Eğilip bir şey alıyor ya da bir el hareketi yapıyor (bu sırada yürüyemez)
	bool IsBusy() const { return PickupTimer >= 0.f || HandAction != ENLHandAction::None; }

	/**
	 * El hareketi oynat. Target: elin gideceği yer (dünya). TargetRotation: şarjdaki fenerin / dökülen bidonun açısı.
	 * PeakFraction anında OnPeak çağrılır (şalter döner, fener takılır, benzin eklenir).
	 * SwitchDirection: şalter için +1 yukarı, -1 aşağı.
	 */
	void PlayHandAction(ENLHandAction Type, const FVector& Target, const FRotator& TargetRotation, float Duration, float PeakFraction,
		TFunction<void()> OnPeak, float SwitchDirection = 0.f);

	// Test: konsola "NLDebugShots" yaz -> fener, eğilme, taşıma, oturma pozlarının ekran görüntüleri alınır
	UFUNCTION(Exec) void NLDebugShots();
	// Test: şalter kolu (açık/attı/tekrar açık) ve yürürken fener tutan elin ekran görüntüleri
	UFUNCTION(Exec) void NLDebugShots2();
	// Test: Boğulmuş belirme, yüz siperi, kaçış, kaybolma (gölge) ve jumpscare görüntüleri
	UFUNCTION(Exec) void NLDebugShots3();
	// Test: Runner kovalaması ve yakalama jumpscare görüntüleri
	UFUNCTION(Exec) void NLDebugShots4();
	ANLSecurityDesk* GetCurrentDesk() const { return CurrentDesk.Get(); }
	ANLGenerator* GetCurrentGenerator() const { return CurrentGenerator.Get(); }
	UNLFlashlightComponent* GetFlashlight() const { return Flashlight; }
	FText GetCurrentInteractPrompt() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLFlashlightComponent> Flashlight;
	// Görünen gövde (GetMesh() görünmez "sürücü": yürüme animasyonunu oynatır)
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLBodyRigComponent> BodyRig;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> FlashlightPivot;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> FlashlightMesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> CarryCanPivot;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> CarryCanMesh;

	// --- Gövde / kamera ---
	// Gövde kameranın biraz arkasında durur ki aşağı bakınca boyun içi görünmesin
	UPROPERTY(EditAnywhere, Category = "NL|Body") float BodyBackOffset = 14.f;
	UPROPERTY(EditAnywhere, Category = "NL|Body") float StandEyeHeight = 64.f;
	// Oturunca gözün kalçadan yüksekliği
	UPROPERTY(EditAnywhere, Category = "NL|Body") float SeatedEyeAbovePelvis = 64.f;
	UPROPERTY(EditAnywhere, Category = "NL|Body") float SitDuration = 1.5f;
	UPROPERTY(EditAnywhere, Category = "NL|Body") float StandDuration = 1.7f;
	// Kalkınca sandalyeden ne kadar geri çekilir
	UPROPERTY(EditAnywhere, Category = "NL|Body") float StandStepBack = 40.f;
	UPROPERTY(EditAnywhere, Category = "NL|Body") float PickupDuration = 1.1f;
	UPROPERTY(EditAnywhere, Category = "NL|Body") float PickupCrouchDepth = 50.f;

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

	// Oturma / kalkma
	enum class ESitPhase : uint8 { None, SittingDown, Seated, StandingUp };
	ESitPhase SitPhase = ESitPhase::None;
	float SitTimer = 0.f;
	float SitAlpha = 0.f;
	FVector SitFromLocation = FVector::ZeroVector;
	FVector SitPelvis = FVector::ZeroVector;
	FVector SitForward = FVector::ForwardVector;
	float SitFloorZ = 0.f;
	float SitDeskHeight = 76.f;
	FVector SitLookAt = FVector::ZeroVector;

	// Eğilip alma
	float PickupTimer = -1.f;
	FVector PickupCanTop = FVector::ZeroVector;
	FRotator PickupLookRotation = FRotator::ZeroRotator;

	float FlashAim = 0.f;
	float WorkAlpha = 0.f;

	// El hareketi
	ENLHandAction HandAction = ENLHandAction::None;
	float ActionTimer = 0.f;
	float ActionDuration = 1.f;
	float ActionPeak = 0.5f;
	bool bActionPeakDone = false;
	TFunction<void()> ActionOnPeak;
	FVector ActionTarget = FVector::ZeroVector;
	FRotator ActionRotation = FRotator::ZeroRotator;
	float ActionSwitchDir = 0.f;
	bool bCanStillVisible = false;  // döküm bitene kadar bidon elde görünür

	// Şarjdaki fenerin yeri
	bool bFlashlightRestValid = false;
	FTransform FlashlightRestTransform = FTransform::Identity;

	void UpdateBody(float DeltaSeconds);
	void FinishSitDown();
	void FinishStandUp();

	// NLDebugShots
	UPROPERTY(Transient) TObjectPtr<AActor> DebugCamera;
	bool bDebugWalk = false;
	void DebugShot(const TCHAR* Name);
	void DebugThirdPerson(bool bEnable, const FVector& Offset = FVector(200.f, 70.f, 0.f));

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
	// Sunum kısayolu (geliştirme sürümü): F9 -> bu gün 5 sn sonra biter
	UPROPERTY(Transient) TObjectPtr<UInputAction> DebugEndDayAction;

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
	void OnDebugEndDay();

	void UpdateFocus();
	void UpdateFootsteps(float DeltaSeconds);
	void CreateHUD();
};
