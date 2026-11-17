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
class ANLFuseBox;
class UNLFuseBoxWidget;
class ANLHideDoor;
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

	// --- Sigorta kutusu arayüzü ---
	void OpenFuseBox(ANLFuseBox* Box);
	void CloseFuseBox();
	void CloseFuseBoxWithBlend(float BlendTime);
	// Şalter açıkken sigortaya dokununca: arayüz kapanır, ekran elektriklenir ve sallanır
	void ElectricShock();
	// 0-1: elektrik çarpması efektinin şiddeti (HUD kullanır)
	float GetShockAlpha() const { return ShockDuration > 0.f ? FMath::Clamp(ShockTime / ShockDuration, 0.f, 1.f) : 0.f; }
	UNLFuseBoxWidget* GetFuseBoxWidget() const { return FuseWidget; }

	// --- Saklanma kapısı (Runner kapıyı zorlarken) ---
	void EnterDoorHold(ANLHideDoor* Door);
	void ExitDoorHold();
	ANLHideDoor* GetHoldDoor() const { return HoldDoor.Get(); }

	// --- Koşma / stamina ---
	float GetStamina() const { return Stamina; }
	bool IsSprinting() const { return bSprinting; }
	bool IsExhausted() const { return bExhausted; }
	bool IsStaminaDepleted() const { return bDepleted; }

	// --- Benzin ---
	bool HasFuelCan() const { return bHasFuelCan; }
	// CanTop: bidonun dünyadaki tutma noktası (oyuncu eğilip oradan alır)
	void PickupFuelCan(float Amount, const FVector& CanTop);
	void SetCanSource(class ANLFuelCan* Can);
	// G: elindeki bidonu önüne yere bırak
	void DropFuelCan();
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
	// Test: etkileşim işaretleri (uzakta yuvarlak, bakınca E) arayüzlü ekran görüntüleri
	UFUNCTION(Exec) void NLDebugShots5();
	// Test: tamir hatası -> Runner uzakta bekler, tamirden çıkınca koşar; oyuncu kaçar
	UFUNCTION(Exec) void NLDebugRunnerRepair();
	// Test: Sabotajcı şalteri indirirken (sinsice yürüme, kurcalama, kaçış) ekran görüntüleri
	UFUNCTION(Exec) void NLDebugShots6();
	// Test: sigorta kutusu (sabotaj, elektrik çarpması, düzeltme) ekran görüntüleri
	UFUNCTION(Exec) void NLDebugFuseUI();
	// Test: gerçek fare girişiyle (imleç + sol tuş) sigorta sürükleme ve şalter kolu
	UFUNCTION(Exec) void NLDebugFuseMouse();
	// Test: artı ve odak (dibinde, yanında, boşlukta) arayüzlü ekran görüntüleri
	UFUNCTION(Exec) void NLDebugFocusShots();
	// Test: oyuncuyu bir yere koy ve bir yöne baktır (X Y Pitch Yaw), ardından ekran görüntüsü
	UFUNCTION(Exec) void NLDebugLook(float X, float Y, float Pitch, float Yaw);
	// Test: saklanma kapısı (Win 1: kontroller başarılı, 0: başarısız -> jumpscare) ekran görüntüleri
	UFUNCTION(Exec) void NLDebugDoor(int32 Win);
	// Test: stamina göstergesi (yüzde 75, 35, tükenmiş) ekran görüntüleri
	UFUNCTION(Exec) void NLDebugStamina();
	// Test: Saved/tour.txt içindeki her satır "x y z pitch yaw isim" için uçarak o noktaya gidip ekran görüntüsü alır, sonra çıkar
	UFUNCTION(Exec) void NLDebugTour();
	// Test: başlangıç, masaya oturup 8 kamerayı sırayla gösterme, jeneratör (ekran görüntüleri)
	UFUNCTION(Exec) void NLDebugAda();
	// Test: oyuncu (X,Y) noktasında beklerken Runner kovalar; mesafe ve yol loglanır
	UFUNCTION(Exec) void NLDebugChase(float X, float Y);
	// Test: kulübe pusu alanına gir (kesin pusu), çığlık, Runner yaklaşınca kulübeye kapan ve kapıyı tut
	UFUNCTION(Exec) void NLDebugAmbush();
	// Test: oyuncu dışarıda kameranın yanında; Drowned koşarak gelir, el feneriyle kovalanır, hattan geri koşup kaybolur
	UFUNCTION(Exec) void NLDebugDrowned(int32 CamIndex);
	// Test: her kamera için koşarak gelme hattı bulunuyor mu (loglar)
	UFUNCTION(Exec) void NLDebugDrownedRoutes();
	// Test: bidonu al, jeneratör binasının benzin ağzına dök, arıza + tamir
	UFUNCTION(Exec) void NLDebugRefuel();
	// Test: oyuncu dışarıdayken Drowned gelip gider, kaçırmalar sayılmaz (oyuncu ölmemeli)
	UFUNCTION(Exec) void NLDebugDrownedAway();
	UFUNCTION(Exec) void NLDebugChaos();
	UFUNCTION(Exec) void NLDebugDiag();
	// Test: ses atmosferi (içeride/dışarıda, uzaktan sesler loglanır)
	UFUNCTION(Exec) void NLDebugAmbience();
	// Test: yağmur (dışarı/şimşek/içeri/2. gece dinmesi) ekran görüntüleri
	UFUNCTION(Exec) void NLDebugRainShots();
	// Test: açılış sinematiği boyunca ekran görüntüleri (oyunu -NLIntro ile başlat)
	UFUNCTION(Exec) void NLDebugIntroShots();
	// Test: Lvl_Ada_Art turu - noktalarda ekran görüntüsü + kare süresi (ms)
	UFUNCTION(Exec) void NLDebugArtTour();
	// Test: deniz feneri, içi, kontrol odası tozu, rıhtımdan huzme ve yol fenerleri
	UFUNCTION(Exec) void NLDebugLighthouseTour();
	// Test: benzin bidonunu al, yavaşlamayı logla, G ile bırak, tekrar al
	UFUNCTION(Exec) void NLDebugDropCan();
	// Test: duraklatma menüsünü aç, görüntü al, kapat
	UFUNCTION(Exec) void NLDebugPauseMenu();
	// Fener elde yürürken bilek ve fener modeli: yakın 3. şahıs + 1. şahıs kareleri
	UFUNCTION(Exec) void NLDebugHandShots();
	// Yol fenerleri: her fenere bir öncekinin yanından göz hizasından bakan kareler + üstten görünüm
	UFUNCTION(Exec) void NLDebugGuideShots();
	// Fener ve deniz feneri teklemesi: kısa aralıkla tekletip ışık şiddetini loglar
	UFUNCTION(Exec) void NLDebugStutter();
	// Performans: tipik oyun noktalarında kare/oyun/çizim/GPU süresi + ProfileGPU dökümü
	UFUNCTION(Exec) void NLDebugPerf();
	// Jeneratör modeli: dışarıdan görünüm, benzin ağzı yakın plan, arıza (kırmızı lamba)
	UFUNCTION(Exec) void NLDebugGenShots();

	// Sinematik (açılış): arayüz gizli, oyuncu kontrolü kapalı
	void SetCinematicMode(bool bOn);
	void RestoreGameInput();
	bool IsInCinematic() const { return bCinematic; }
	ANLSecurityDesk* GetCurrentDesk() const { return CurrentDesk.Get(); }
	ANLGenerator* GetCurrentGenerator() const { return CurrentGenerator.Get(); }
	UNLFlashlightComponent* GetFlashlight() const { return Flashlight; }
	FText GetCurrentInteractPrompt() const;
	// Şu an bakılan (E menzilindeki) etkileşimli nesne
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

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
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<class UAudioComponent> PantingAudio;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<class UAudioComponent> ExhaustedBreathAudio;

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

	// Nesnenin görünen sınırına bu mesafeden (cm) yakınsan E ile etkileşebilirsin
	UPROPERTY(EditAnywhere, Category = "NL|Player") float InteractDistance = 280.f;
	// Artı nesnenin dışında kalsa da bu açı (derece) kadar yakınsa nesne seçilir (küçük nesneler kolay hedeflensin)
	UPROPERTY(EditAnywhere, Category = "NL|Player") float InteractAimTolerance = 7.f;
	UPROPERTY(EditAnywhere, Category = "NL|Player") float WalkSpeed = 280.f;
	UPROPERTY(EditAnywhere, Category = "NL|Player") float MouseSensitivity = 0.6f;

	// Shift ile koşma (Runner 250 cm/sn)
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float SprintSpeed = 480.f;
	// Benzin bidonu taşırken yürüme ve koşma hızı çarpanı
	UPROPERTY(EditAnywhere, Category = "NL|Player") float CarryCanSpeedMultiplier = 0.82f;

	// Kapı tutarken: bakış kolun ne kadar üstüne (cm; küçüldükçe daha aşağı bakar) ve gövdenin kapıya doğru eğilmesi (derece)
	UPROPERTY(EditAnywhere, Category = "NL|Door") float HoldLookAboveGrip = 35.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float HoldLean = 8.f;
	// Tam stamina ile kaç saniye koşulabilir
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float SprintDuration = 5.5f;
	// Boştan doluya dolma süresi (sn) ve koşmayı bıraktıktan sonra dolmaya başlama gecikmesi
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float StaminaRegenTime = 7.f;
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float StaminaRegenDelay = 0.9f;
	// Stamina tükenince bu orana dolana kadar tekrar koşamaz
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float ExhaustedRecover = 0.3f;
	// Stamina tamamen biterse, tekrar dolana kadar dolma hızı bu oranla çarpılır (bar soluk gri)
	UPROPERTY(EditAnywhere, Category = "NL|Sprint", meta = (ClampMin = "0.05", ClampMax = "1")) float DepletedRegenMultiplier = 0.45f;
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") TObjectPtr<USoundBase> PantingLoop;
	// Stamina bitince: derin, hırıltılı nefes (döngü) ve ekran nefesle birlikte gri-normal-gri atar
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") TObjectPtr<USoundBase> ExhaustedBreathLoop;
	// Bir nefesin süresi (sn): sesle aynı olmalı (breath_exhausted_loop: 1.1)
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float ExhaustedBreathPeriod = 1.1f;
	// Griliğin en koyu anındaki renk doygunluğu (0 = tamamen gri)
	UPROPERTY(EditAnywhere, Category = "NL|Sprint") float ExhaustedSaturation = 0.08f;

	UPROPERTY(EditAnywhere, Category = "NL|UI") TSubclassOf<UNLHUDWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> SitMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> StandMontage;
	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> FootstepSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") float FootstepInterval = 0.55f;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> StandUpSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> DeathSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ShockSound;

	// Elektrik çarpması: süre (sn) ve bakış sarsıntısı (derece)
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float ShockDuration = 0.9f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float ShockShake = 3.5f;

	// Kapıyı tutarken panik nefesi: bakış sallanması (derece) ve nefes hızı (nefes/sn)
	UPROPERTY(EditAnywhere, Category = "NL|Door") float BreathShake = 1.1f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float BreathRate = 2.1f;

private:
	ENLPlayerMode Mode = ENLPlayerMode::Walking;
	TWeakObjectPtr<ANLSecurityDesk> CurrentDesk;
	TWeakObjectPtr<ANLGenerator> CurrentGenerator;
	TWeakObjectPtr<AActor> FocusedActor;
	bool bHasFuelCan = false;
	bool bDroppingCan = false;
	bool bDropDone = false;
	FVector DropSpot = FVector::ZeroVector;
	TWeakObjectPtr<class ANLFuelCan> CanSource;
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

	// Sigorta kutusu
	UPROPERTY(Transient) TObjectPtr<UNLFuseBoxWidget> FuseWidget;
	TWeakObjectPtr<ANLFuseBox> CurrentFuseBox;
	float ShockTime = 0.f;
	FRotator ShockShakeOffset = FRotator::ZeroRotator;

	// Saklanma kapısı
	TWeakObjectPtr<ANLHideDoor> HoldDoor;
	FVector HoldFromLocation = FVector::ZeroVector;
	float HoldTimer = 0.f;
	float HoldHands = 0.f;
	float BreathAlpha = 0.f;
	float BreathTime = 0.f;
	FRotator BreathOffset = FRotator::ZeroRotator;

	// Koşma
	float Stamina = 1.f;
	bool bSprinting = false;
	bool bExhausted = false;
	bool bDepleted = false;
	float StaminaRegenTimer = 0.f;
	float PantingVolume = 0.f;
	bool bHoldGripRight = false;
	float ExhaustAlpha = 0.f;
	float ExhaustPhase = 0.f;
	bool bWasExhausted = false;
	float AppliedSaturation = 1.f;
	void UpdateStamina(float DeltaSeconds);

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
	bool bCinematic = false;
	uint64 TourFrame0 = 0;
	double TourTime0 = 0.0;

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
	UPROPERTY(Transient) TObjectPtr<UInputAction> DropAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PauseAction;
	UPROPERTY(Transient) TObjectPtr<class UNLPauseMenuWidget> PauseMenu;
	void OnPause(const struct FInputActionValue& Value) { TogglePause(); }
public:
	// ESC / P: duraklatma menüsü
	void TogglePause();
	bool IsPaused() const { return PauseMenu != nullptr; }
protected:
	void OnDrop(const struct FInputActionValue& Value) { DropFuelCan(); }

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
	TArray<TWeakObjectPtr<AActor>> InteractableCache;
	float InteractableCacheTimer = 0.f;
	void UpdateFootsteps(float DeltaSeconds);
	void CreateHUD();
};
