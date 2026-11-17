// NoLightAfterMidnight - Sigorta kutusu: ana güç şalteri + 4 renkli sigorta
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLTypes.h"
#include "NLFuseBox.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UCameraComponent;
class UPointLightComponent;
class USoundBase;
class UAnimMontage;

/**
 * Elektrik ancak şalter yukarıdaysa (açık) VE 4 sigortanın her biri kendi renginde yuvadaysa gelir.
 * Sabotajcı sigortaların yerini karıştırır; şalteri bazen indirir, bazen açık bırakır.
 * Oyuncu E ile sigorta arayüzünü açar: şalteri kapatıp sigortaları doğru yuvalara taşır, sonra şalteri açar.
 * Şalter açıkken sigortaya dokunursa elektrik çarpar.
 *
 * Model: Scripts/blender_fusebox.py (SM_NL_FuseCabinet ve parçaları). Yuvalar: 0-2 üst sıra dikey (CAM 1-3), 3 ortada yatay (FLASH).
 * Göstergeler ve yeşil LED'ler elektrik durumunu gösterir (hangi sigortanın yanlış olduğunu söylemez).
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLFuseBox : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLFuseBox();
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

	static constexpr int32 NumFuses = 4;

	UFUNCTION(BlueprintPure, Category = "NL|Fuse") bool IsOn() const { return bOn; }
	UFUNCTION(BlueprintPure, Category = "NL|Fuse") bool AreFusesCorrect() const;
	// Elektrik verebilir mi: şalter açık ve sigortalar doğru
	UFUNCTION(BlueprintPure, Category = "NL|Fuse") bool IsPowerOk() const { return bOn && AreFusesCorrect(); }
	// Sabotaj sonrası (sigortalar karışık) -> HUD uyarısı
	UFUNCTION(BlueprintPure, Category = "NL|Fuse") bool IsTripped() const { return !AreFusesCorrect(); }

	// Yuvadaki sigortanın rengi (sigorta numarası = doğru yuvası)
	int32 GetFuseInSlot(int32 Slot) const { return SlotFuse.IsValidIndex(Slot) ? SlotFuse[Slot] : Slot; }
	// Yuvanın istediği sigorta rengi (kablolama). Kutuda yazmaz; sadece güvenlik masasındaki monitörde görünür.
	int32 GetTargetFuse(int32 Slot) const { return SlotTarget.IsValidIndex(Slot) ? SlotTarget[Slot] : Slot; }
	static FLinearColor GetFuseColor(int32 Fuse);
	static FText GetFuseColorName(int32 Fuse);
	// Yuvanın kutudaki etiketi (CAM 1, CAM 2, CAM 3, FLASH) - monitördeki kablolama listesi de bunu kullanır
	static FString GetSlotLabel(int32 Slot);

	UFUNCTION(BlueprintCallable, Category = "NL|Fuse") void SetOn(bool bNewOn);
	// İki yuvadaki sigortaların yerini değiştirir (arayüz)
	void SwapFuses(int32 SlotA, int32 SlotB);
	// Sabotaj: sigortaları karıştırır, şalteri rastgele indirir ya da açık bırakır
	UFUNCTION(BlueprintCallable, Category = "NL|Fuse") void Sabotage();
	// Şalterin inip inmeyeceği belli sabotaj (test)
	void SabotageWith(bool bSwitchOff);
	// Eski API: artık sabotaj yapar
	UFUNCTION(BlueprintCallable, Category = "NL|Fuse") void Trip() { Sabotage(); }

	// --- Kutunun üstünde fareyle etkileşim (oyuncu E ile yakın plana geçince) ---
	// Fare ışınının değdiği sigortanın yuvası (yoksa INDEX_NONE)
	int32 RayPickFuse(const FVector& Origin, const FVector& Dir) const;
	// Işının panel yüzeyinde değdiği noktaya en yakın yuva (yoksa INDEX_NONE)
	int32 RayPickSlot(const FVector& Origin, const FVector& Dir) const;
	bool RayHitsLever(const FVector& Origin, const FVector& Dir) const;
	// Sürükleme: sigorta panelin üstünde fareyi takip eder
	void BeginDrag(int32 Slot);
	void UpdateDrag(const FVector& Origin, const FVector& Dir);
	void EndDrag();
	// Fare üstündeki sigorta hafifçe büyür
	void SetHoveredSlot(int32 Slot);

	// Şalter kolunu fareyle sürükleme: kol fareyi takip eder; bırakınca yarıyı geçtiyse o tarafa geçer
	float GetLeverAngle() const { return LeverAngle; }
	float GetLeverOnAngle() const { return LeverOnAngle; }
	float GetLeverOffAngle() const { return LeverOffAngle; }
	void SetLeverDragAngle(float AngleDeg);
	void EndLeverDrag();
	FVector GetSlotWorldLocation(int32 Slot) const;
	FVector GetLeverWorldLocation() const;
	// Oyuncu yakın plandayken kutunun üstündeki loş çalışma lambası yanar
	void SetViewing(bool bViewing);

	// Test için: sabotaj bu kadar saniye sonra başlasın
	void SetTripTimer(float Seconds) { TripTimer = Seconds; }

	UPROPERTY(BlueprintAssignable, Category = "NL|Fuse")
	FNLOnPowerChanged OnSwitched;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> BoxMesh;
	// Şalter kolu, açık/kapalı açıları arasında döner
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> LeverMesh;
	// Kartuş sigortalar (seramik gövde + metal başlıklar + renkli bant)
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> FuseBodies;
	// Gösterge ibreleri: 0-2 CAM, 3 FLASH, 4 ana gösterge
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> Needles;
	// Yeşil LED'ler: 0-2 CAM sigortalarının üstünde, 3 FLASH
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> Leds;
	UPROPERTY() TObjectPtr<UStaticMesh> CabinetAsset;
	UPROPERTY() TObjectPtr<UStaticMesh> LeverAsset;
	// E ile açılınca oyuncunun görüşü bu kameraya geçer (sigortalar ve şalter yakın planda)
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UCameraComponent> FuseViewCamera;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UPointLightComponent> ViewLight;

	UPROPERTY(EditAnywhere, Category = "NL|Fuse") bool bStartOn = true;
	// Sabotajcı zaman zaman gelir. Sadece gün sürerken ve elektrik varken sayar.
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") bool bRandomTrips = true;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse", meta = (ClampMin = "5")) float MinTripInterval = 120.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse", meta = (ClampMin = "5")) float MaxTripInterval = 240.f;
	// Sabotajcının şalteri indirme olasılığı (yoksa açık bırakır: dokunan çarpılır)
	UPROPERTY(EditAnywhere, Category = "NL|Fuse", meta = (ClampMin = "0", ClampMax = "1")) float SabotageSwitchOffChance = 0.5f;
	// Her sabotajda doğru dizilim (kablolama) değişir; yeni sıra sadece masadaki monitörde yazar
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") bool bRandomizeWiring = true;
	// Sigortalar yanlışken şalter kaldırılırsa kol bu kadar sn sonra kıvılcımla geri düşer
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float WrongWiringKickback = 0.35f;

	// Şalter kolu: kutunun ön yüzündeki mafsaldan dışarı çıkar. Açıkken yukarı, kapalıyken aşağı bakar.
	// LeverPivot ve LeverOutward kutunun (BoxMesh) kendi koordinatlarında.
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FVector LeverPivot = FVector(5.f, 7.f, 6.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FVector LeverOutward = FVector(0.f, 1.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverLength = 14.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverOnAngle = 55.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverOffAngle = -55.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverSpeed = 400.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SwitchOnSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SwitchOffSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> TripSound;     // sabotaj
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> FuseMoveSound; // sigorta takıp çıkarma

	UPROPERTY(EditAnywhere, Category = "NL|Animation") TObjectPtr<UAnimMontage> PlayerSwitchMontage;

private:
	bool bOn = true;
	bool bWasPowerOk = true;
	float LeverAngle = 0.f;
	float TripTimer = 0.f;
	TArray<int32> SlotFuse;
	TArray<int32> SlotTarget;
	FTimerHandle KickbackTimer;
	int32 DraggedSlot = INDEX_NONE;
	int32 HoveredSlot = INDEX_NONE;
	bool bLeverDragging = false;
	void PlaceFuseMeshes(int32 SlotIdx, const FVector& LocalPos, float Scale, bool bHorizontal);
	// Model parçalarını ve yerleşimi uygular (leveldeki eski küp ayarlarını ezer)
	void ApplyModel();
	void TickGauges(float DeltaSeconds);
	TArray<float> NeedleAngles;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LedMIDs;
	void ResetFuseMeshes();
	bool RayToPanel(const FVector& Origin, const FVector& Dir, FVector& OutLocal) const;

	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FuseMIDs;

	void PlaceLever(float AngleDeg);
	void ScheduleNextTrip();
	void ApplyVisuals();
	void ApplyFuseVisuals();
	void RefreshPower();
};
