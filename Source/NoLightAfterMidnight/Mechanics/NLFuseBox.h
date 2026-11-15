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
 * Yuva sırası (karşıdan bakınca): 0 sol üst, 1 sağ üst, 2 sol alt, 3 sağ alt.
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
	static FLinearColor GetFuseColor(int32 Fuse);
	static FText GetFuseColorName(int32 Fuse);

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

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> BoxMesh;
	// Şalter kolu, açık/kapalı açıları arasında döner
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> LeverMesh;
	// Kutunun soluna uzanan sigorta paneli ve üstündeki 4 sigorta (gövde + iki metal başlık) ile renkli yuva plakaları
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> PanelMesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> SlotPlates;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> FuseBodies;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> FuseCaps;
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

	// Şalter kolu: kutunun ön yüzündeki mafsaldan dışarı çıkar. Açıkken yukarı, kapalıyken aşağı bakar.
	// LeverPivot ve LeverOutward kutunun (BoxMesh) kendi koordinatlarında.
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FVector LeverPivot = FVector(50.f, 100.f, 50.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") FVector LeverOutward = FVector(0.f, 1.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverLength = 30.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverOnAngle = 45.f;
	UPROPERTY(EditAnywhere, Category = "NL|Fuse") float LeverOffAngle = -45.f;
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
	int32 DraggedSlot = INDEX_NONE;
	int32 HoveredSlot = INDEX_NONE;
	bool bLeverDragging = false;
	void PlaceFuseMeshes(int32 SlotIdx, const FVector& LocalPos, float Scale);
	void ResetFuseMeshes();
	bool RayToPanel(const FVector& Origin, const FVector& Dir, FVector& OutLocal) const;

	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> FuseMIDs;

	void PlaceLever(float AngleDeg);
	void ScheduleNextTrip();
	void ApplyVisuals();
	void ApplyFuseVisuals();
	void RefreshPower();
};
