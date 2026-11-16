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
	UAudioComponent* GetAmbientAudio() const { return AmbientAudio; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") int32 GetTotalDays() const { return TotalDays; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") float GetDayTimeRemaining() const { return DayTimeRemaining; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") float GetDayProgress() const { return DayLengthSeconds > 0.f ? 1.f - DayTimeRemaining / DayLengthSeconds : 1.f; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") bool IsDayActive() const { return bDayActive && !bGameOver; }
	UFUNCTION(BlueprintPure, Category = "NL|Game") bool IsGameOver() const { return bGameOver; }

	// ScreenDelay: jumpscare animasyonu bitsin diye game over ekranı bu kadar sonra açılır
	UFUNCTION(BlueprintCallable, Category = "NL|Game")
	void TriggerGameOver(ENLGameOverReason Reason, float ScreenDelay = 1.f);

	// Jeneratör tamirinde hata yapılınca çağrılır
	UFUNCTION(BlueprintCallable, Category = "NL|Game")
	void RequestRunnerChase(ANLPlayerCharacter* Target);

	// Test: konsola "NLDebugRunner" yaz -> Runner hemen oyuncuyu kovalar
	UFUNCTION(Exec)
	void NLDebugRunner();

	// Test: konsola "NLDebugTimeScale 20" yaz -> oyun zamanı 20 kat hızlanır (arıza/şalter zamanlamasını denemek için)
	UFUNCTION(Exec)
	void NLDebugTimeScale(float Scale);

	// Sunum: konsola "NLDebugDayLength 5" yaz -> bundan sonra her gün 5 sn sürer (saat kaldığı yerden devam eder)
	UFUNCTION(Exec)
	void NLDebugDayLength(float Seconds);

	// Sunum: "NLDebugEndDayIn 5" -> bu gün 5 sn sonra biter (F9 kısayolu da bunu yapar)
	UFUNCTION(Exec)
	void NLDebugEndDayIn(float Seconds);

	// Test: konsola "NLDebugFuseIn 5" yaz -> şalter 5 sn sonra kendiliğinden atar
	UFUNCTION(Exec)
	void NLDebugFuseIn(float Seconds);

	// Test: "NLDebugGeneratorIn 5" -> jeneratör 5 sn sonra bozulur (şalter arızasıyla çakışma kuralını denemek için)
	UFUNCTION(Exec)
	void NLDebugGeneratorIn(float Seconds);

	// Test: "NLDebugRain 1" / "NLDebugRain 0" -> yağmuru aç/kapat (yavaşça)
	UFUNCTION(Exec)
	void NLDebugRain(int32 bOn);

	// Test: "NLDebugLightning" -> hemen şimşek çakar, ardından gök gürler
	UFUNCTION(Exec)
	void NLDebugLightning();

	// Açılış sinematiği bitti: 1. gün başlar ("DAY 1")
	void FinishIntro();

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
	// Oyun başında NavMesh'i bir kez tam kur (Runner'ın yol bulması için). Level'de önceden kurulmuş NavMesh varsa kapatılabilir.
	UPROPERTY(EditAnywhere, Category = "NL|Game") bool bBuildNavMeshOnStart = true;
	// Level'de yoksa ses atmosferi yöneticisini (ANLAmbienceDirector) oluştur
	UPROPERTY(EditAnywhere, Category = "NL|Audio") bool bSpawnAmbience = true;
	// Level'de yoksa hava durumu yöneticisini (ANLWeatherDirector: yağmurlu geceler) oluştur
	UPROPERTY(EditAnywhere, Category = "NL|Game") bool bSpawnWeather = true;
	// Level'de ANLIntroDirector varsa oyun botla rıhtıma yanaşma sinematiğiyle açılır (ölüp yeniden başlayınca oynamaz)
	UPROPERTY(EditAnywhere, Category = "NL|Game") bool bPlayIntro = true;
	// Rıhtımdan deniz fenerine giden patikaya yanan fenerli direkler + tabela (NLGuideLanterns)
	UPROPERTY(EditAnywhere, Category = "NL|Game") bool bGuideLanterns = true;

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
	void BeginIntroOrDay();
	void EndDay();
	void ShowGameOverScreen(ENLGameOverReason Reason);
	void RestartLevel();
};
