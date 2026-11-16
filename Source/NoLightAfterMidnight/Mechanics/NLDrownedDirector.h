// NoLightAfterMidnight - Drowned'ın beynini yöneten aktör (sahneye 1 tane koyun)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLTypes.h"
#include "NLDrownedDirector.generated.h"

class ANLDrowned;
class ANLSecurityDesk;
class ANLSecurityCamera;
class USoundBase;

/**
 * Döngü (şemadaki üst satır):
 *  Waiting -> bir kamerada belirir (Watching)
 *  Watching: oyuncu o kameraya bakıp SPACE ile ışık yakarsa -> Flinching (elini yüzüne kapatır, kaçar)
 *            süre dolarsa -> saldırır -> GAME OVER
 *  Flinching bitince: kaçış sayacı +1. 3 olursa haritadan ayrılır (Away, 2 dk), değilse başka kameraya geçer.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLDrownedDirector : public AActor
{
	GENERATED_BODY()

public:
	ANLDrownedDirector();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "NL|Drowned") ENLDrownedState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "NL|Drowned") int32 GetEscapeCount() const { return EscapeCount; }
	UFUNCTION(BlueprintPure, Category = "NL|Drowned") ANLSecurityCamera* GetCurrentCamera() const;

protected:
	virtual void BeginPlay() override;

	// Sahneye yerleştirdiğiniz Drowned (mesh/anim Details panelinden atanır). Boşsa DrownedClass spawn edilir.
	UPROPERTY(EditInstanceOnly, Category = "NL|Setup") TObjectPtr<ANLDrowned> PlacedDrowned;
	UPROPERTY(EditAnywhere, Category = "NL|Setup") TSubclassOf<ANLDrowned> DrownedClass;
	UPROPERTY(EditInstanceOnly, Category = "NL|Setup") TObjectPtr<ANLSecurityDesk> Desk;

	// Gün başında ilk belirme
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float FirstAppearanceDelay = 40.f;
	// İlk gece daha uzun: oyuncu rıhtımdan fenere yürüyüp etrafa baksın
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float FirstNightAppearanceDelay = 120.f;
	// Kaçtıktan sonra başka kamerada belirme aralığı
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float MinRelocateDelay = 6.f;
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float MaxRelocateDelay = 15.f;
	// Belirdikten sonra oyuncunun ışık tutmak için süresi
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float ReactionTime = 10.f;
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float ReactionTimeReductionPerDay = 1.f;
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float MinReactionTime = 4.f;
	// 3 kaçıştan sonra haritadan ayrılma süresi (2 dk)
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float AwayDuration = 120.f;

	UPROPERTY(EditAnywhere, Category = "NL|Rules") int32 EscapesBeforeLeaving = 3;
	// Üst üste kaç kez ışık tutulmazsa saldırır. 2 = ilk kaçırmada uyarı sesi + başka kameraya geçer, ikincide saldırır. Başarılı ışık sayacı sıfırlar.
	UPROPERTY(EditAnywhere, Category = "NL|Rules", meta = (ClampMin = "1")) int32 MissesBeforeAttack = 2;
	// Saldırı animasyonundan sonra game over ekranına geçiş
	UPROPERTY(EditAnywhere, Category = "NL|Rules") float ExtraDelayAfterJumpscare = 0.3f;
	// Oyuncu dışarıdayken el fenerini Drowned'a tutarsa kovalar: menzil (cm) ve ne kadar süre tutulması gerektiği (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Rules") float FlashlightRepelRange = 1800.f;
	UPROPERTY(EditAnywhere, Category = "NL|Rules") float FlashlightRepelTime = 0.35f;
	// Koşarak gelme bu süreyi aşarsa yerine ışınlanır (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float MaxArriveTime = 20.f;
	// Kaos kuralı: koşucu kovalarken / bağırıp gelirken ya da elektrik arızalıyken (jeneratör bozuk, şalter atmış, yakıt bitti)
	// Drowned gelmez; kameradaysa geri koşup çekilir. Her şey düzelince bu kadar sn sakin kalınca tekrar gelebilir.
	UPROPERTY(EditAnywhere, Category = "NL|Rules") bool bHoldDuringChaos = true;
	UPROPERTY(EditAnywhere, Category = "NL|Timing") float CalmDelayAfterChaos = 12.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> LeaveMapSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ReturnSound;
	// Işık tutulmayınca atak gelmeden önce uyarı (kaçırma sayısı < limit iken)
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> MissWarningSound;

	UFUNCTION() void HandleDayStarted(int32 Day);
	UFUNCTION() void HandleDayEnded(int32 Day);
	UFUNCTION() void HandleGameOver(ENLGameOverReason Reason);
	UFUNCTION() void HandleCameraFlashed(ANLSecurityCamera* Camera);

private:
	UPROPERTY(Transient) TObjectPtr<ANLDrowned> Drowned;

	ENLDrownedState State = ENLDrownedState::Inactive;
	float StateTimer = 0.f;
	int32 CurrentCameraIndex = INDEX_NONE;
	int32 EscapeCount = 0;
	int32 MissCount = 0;
	int32 CurrentDay = 1;
	bool bFleePhase = false;
	float FlashlightLitTime = 0.f;
	float CalmTimer = 0.f;
	bool bChaosLogged = false;
	// Koşucu kovalıyor mu ya da elektrik arızası var mı (OutWhy: log için sebep)
	bool IsChaos(const TCHAR*& OutWhy) const;
	void StartFlinch();
	bool IsLitByPlayerFlashlight() const;

public:
	ANLDrowned* GetDrowned() const { return Drowned; }
	// Test: hemen bir kamerada belir (Index < 0: rastgele)
	void DebugAppear(int32 Index);
private:

	void SetState(ENLDrownedState NewState, float Duration);
	void AppearAtRandomCamera();
	void OnReactionTimeExpired();
	void OnFlinchFinished();
	void StartAttack();
	float GetReactionTime() const;
	float RandomRelocateDelay() const;
};
