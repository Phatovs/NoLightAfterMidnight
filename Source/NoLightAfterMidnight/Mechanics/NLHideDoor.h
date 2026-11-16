// NoLightAfterMidnight - Saklanma kapısı: Runner kovalarken odaya girip kapıyı kapatınca kapıyı tutma mini oyunu
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLHideDoor.generated.h"

class UStaticMeshComponent;
class USoundBase;
class UAudioComponent;
class ANLPlayerCharacter;
class ANLRunner;

/**
 * Bir kapı açıklığına yerleştirin. Aktörün ileri yönü (+X) ODANIN İÇİNE bakar; konum = açıklığın zemin ortası.
 * OpeningWidth/Height açıklığın ölçüsüdür; kapının yanı ve üstü dolgu duvarla kapanır.
 * E: kapıyı aç/kapat. Runner kovalarken oyuncu içerideyken kapı kapanırsa:
 *  kamera kapı koluna döner, panik nefesi başlar, Runner dışarıdan kapıyı zorlar ve SPACE kontrolleri gelir.
 *  Hepsi başarılı -> Runner bırakıp gider. Biri kaçarsa -> kapıyı açar ve jumpscare.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLHideDoor : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLHideDoor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// INLInteractable
	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;
	virtual bool CanInteract(const ANLPlayerCharacter* Player) const override;

	bool IsOpen() const { return bWantOpen; }
	bool IsHoldActive() const { return Phase != EHoldPhase::None; }
	// Oyuncu SPACE'e bastı (kapıyı tutarken)
	void HandleHoldInput();

	// HUD için: kontrol penceresi açık mı, ibre/bölge, ilerleme
	bool IsCheckWindowOpen() const { return Phase == EHoldPhase::Check; }
	float GetNeedlePosition() const { return NeedlePos; }
	float GetZoneStart() const { return ZoneStart; }
	float GetZoneWidth() const { return ZoneWidth; }
	float GetHoldProgress() const { return NumChecks > 0 ? float(ChecksPassed) / NumChecks : 1.f; }

	// Oyuncunun kapıyı tutarken baktığı ve elini koyduğu yerler (dünya)
	FVector GetHandleGripLocation() const;   // iç kol
	FVector GetDoorPalmLocation() const;     // kapının iç yüzü (sol el)
	FVector GetHoldStandLocation(float CapsuleHalfHeight) const;
	FVector GetOutsideLocation() const;      // Runner'ın dışarıda durduğu yer
	// Dışarıdan zorlama şiddeti 0-1 (oyuncunun kamerası sarsılır)
	float GetForceAlpha() const { return ForceAlpha; }

	// Test: kapı tutma sırasını başlat (bAutoWin: kontroller kendiliğinden başarılı / başarısız)
	void DebugStartHold(ANLPlayerCharacter* Player, int32 AutoResult);
	// Test: 1 = kontroller kendiliğinden başarılı
	void SetDebugAutoResult(int32 Result) { DebugAutoResult = Result; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> Hinge;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> DoorMesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> HandlePivotIn;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> HandlePivotOut;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> HandleParts;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> FillLeft;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> FillRight;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> FillTop;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TArray<TObjectPtr<UStaticMeshComponent>> FrameParts;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UAudioComponent> BreathAudio;

	// Açıklık ve kapı ölçüleri (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Door") float OpeningWidth = 300.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float OpeningHeight = 350.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float WallThickness = 100.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float DoorWidth = 120.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float DoorHeight = 230.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float DoorThickness = 6.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float HandleHeight = 100.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float OpenAngle = 95.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float OpenSpeed = 2.4f;   // 1/sn
	UPROPERTY(EditAnywhere, Category = "NL|Door") bool bStartOpen = true;

	// Kapı tutma
	UPROPERTY(EditAnywhere, Category = "NL|Hold") int32 NumChecks = 4;
	// İlk zorlama gelmeden önce (kamera kola döner, nefes nefese)
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float SettleTime = 1.8f;
	// Kontroller arası bekleme (sn, rastgele)
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float GapMin = 0.9f;
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float GapMax = 2.0f;
	// Zorlama başladıktan sonra ibrenin çıkması
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float ForceLeadTime = 0.35f;
	// İbrenin bir uçtan diğerine gitme süresi (ilk kontrol); her kontrolde hızlanır
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float SweepTime = 1.35f;
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float SweepSpeedUp = 0.12f;
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float ZoneWidthStart = 0.2f;
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float ZoneWidthMin = 0.12f;
	// Kapı kırılınca oyuncu ne kadar geri savrulur (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Hold") float BurstKnockback = 115.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> OpenSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> CloseSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> BangSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> HandleSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> BurstSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> HoldSuccessSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> PanicBreathLoop;

private:
	enum class EHoldPhase : uint8 { None, Settle, Gap, Force, Check, Release, Burst };
	EHoldPhase Phase = EHoldPhase::None;
	float PhaseTimer = 0.f;
	float PhaseLength = 0.f;
	int32 ChecksPassed = 0;
	float NeedlePos = 0.f;
	float ZoneStart = 0.5f;
	float ZoneWidth = 0.2f;
	float ForceAlpha = 0.f;
	float HandleDown = 0.f;      // 0-1 (kol aşağı basılı)
	float Rattle = 0.f;          // kapının aralanma titreşimi (derece)
	FVector BurstFrom = FVector::ZeroVector;
	bool bBurstKillDone = false;
	int32 BurstSettledFrames = 0;
	int32 DebugAutoResult = 0;   // 0 yok, 1 hep başarılı, -1 ilk kontrolde başarısız

	bool bWantOpen = true;
	float OpenAlpha = 1.f;       // 0 kapalı, 1 açık
	bool bWasClosing = false;
	TWeakObjectPtr<ANLPlayerCharacter> ClosingPlayer;

	TWeakObjectPtr<ANLPlayerCharacter> HoldPlayer;
	TWeakObjectPtr<ANLRunner> HoldRunner;

	void LayoutParts();
	// Kapı panelinin yerel X'i (duvarın oda tarafı yüzü)
	float GetDoorPlaneX() const { return FMath::Max(WallThickness * 0.5f - DoorThickness * 0.5f - 2.f, 0.f); }
	void ApplyDoorRotation();
	bool IsPlayerInside(const ANLPlayerCharacter* Player) const;
	ANLRunner* FindChasingRunner() const;
	void BeginHold(ANLPlayerCharacter* Player, ANLRunner* Runner);
	void SetPhase(EHoldPhase NewPhase, float Length);
	void StartCheck();
	void PassCheck();
	void FailHold();
	void FinishHold();
	void EndHoldCommon();
};
