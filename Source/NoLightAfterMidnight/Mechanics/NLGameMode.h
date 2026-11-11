// NoLightAfterMidnight - Gün döngüsü (10 dk x 7 gece), game over, kazanma
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NLTypes.h"
#include "NLGameMode.generated.h"

class USoundBase;
class ANLPlayerCharacter;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANLGameMode();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "NL|Game") int32 GetCurrentDay() const { return CurrentDay; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") int32 GetTotalDays() const { return TotalDays; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") float GetDayTimeRemaining() const { return DayTimeRemaining; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") float GetDayProgress() const { return DayLengthSeconds > 0.f ? 1.f - DayTimeRemaining / DayLengthSeconds : 1.f; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") bool IsDayActive() const { return bDayActive && !bGameOver; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") bool IsGameOver() const { return bGameOver; }

	// ScreenDelay: jumpscare animasyonu bitsin diye game over ekranı bu kadar sonra açılır
	UFUNCTION(BlueprintCallable, Category = "NL|Game")
	void TriggerGameOver(ENLGameOverReason Reason, float ScreenDelay = 1.f);

	// Jeneratör tamirinde hata yapılınca çağrılır
	void RequestRunnerChase(ANLPlayerCharacter* Target);

	UPROPERTY(BlueprintAssignable, Category = "NL|Game") FNLOnDayEvent OnDayStarted;
	UPROPERTY(BlueprintAssignable, Category = "NL|Game") FNLOnDayEvent OnDayEnded;
	// Ölüm anı (sistemler durur)
	UPROPERTY(BlueprintAssignable, Category = "NL|Game") FNLOnGameOver OnGameOver;
	// Jumpscare bittikten sonra (HUD game over panelini açar)
	UPROPERTY(BlueprintAssignable, Category = "NL|Game") FNLOnGameOver OnShowGameOverScreen;
	UPROPERTY(BlueprintAssignable, Category = "NL|Game") FNLOnSimpleEvent OnGameWon;

protected:
	virtual void BeginPlay() override;

	// Gün süresi: 10 dakika
	UPROPERTY(EditAnywhere, Category = "NL|Game") float DayLengthSeconds = 600.f;
	// Hikaye: 7 gece
	UPROPERTY(EditAnywhere, Category = "NL|Game") int32 TotalDays = 7;
	// "Day 2" yazısının ekranda kaldığı süre
	UPROPERTY(EditAnywhere, Category = "NL|Game") float DayTransitionDuration = 5.f;
	// Game over ekranından sonra seviye yeniden başlar
	UPROPERTY(EditAnywhere, Category = "NL|Game") float RestartDelay = 6.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> DayStartSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> DayEndSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> GameOverSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> WinSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> AmbientLoop;

private:
	int32 CurrentDay = 0;
	float DayTimeRemaining = 0.f;
	bool bDayActive = false;
	bool bGameOver = false;
	FTimerHandle TransitionTimer;
	FTimerHandle GameOverTimer;

	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> AmbientAudio;

	void StartDay(int32 Day);
	void EndDay();
	void ShowGameOverScreen(ENLGameOverReason Reason);
	void RestartLevel();
};
