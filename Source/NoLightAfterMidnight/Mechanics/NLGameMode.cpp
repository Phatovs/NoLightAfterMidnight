#include "NLGameMode.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "NLRunner.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ANLGameMode::ANLGameMode()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(DayStartSound, "day_start");
	NL_DEFAULT_SOUND(DayEndSound, "day_end");
	NL_DEFAULT_SOUND(GameOverSound, "game_over");
	NL_DEFAULT_SOUND(WinSound, "win");
	NL_DEFAULT_SOUND(AmbientLoop, "ambient_wind_sea_loop");

	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ANLPlayerCharacter::StaticClass();
}

void ANLGameMode::BeginPlay()
{
	Super::BeginPlay();

	AmbientAudio = UGameplayStatics::SpawnSound2D(this, AmbientLoop);

	// Diğer aktörler delegelere bağlansın diye 1 kare sonra Gün 1'i başlat
	GetWorldTimerManager().SetTimerForNextTick([this]() { StartDay(1); });
}

void ANLGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsDayActive()) return;

	DayTimeRemaining -= DeltaSeconds;
	if (DayTimeRemaining <= 0.f)
	{
		DayTimeRemaining = 0.f;
		EndDay();
	}
}

void ANLGameMode::StartDay(int32 Day)
{
	CurrentDay = Day;
	DayTimeRemaining = DayLengthSeconds;
	bDayActive = true;
	UGameplayStatics::PlaySound2D(this, DayStartSound);
	OnDayStarted.Broadcast(CurrentDay);
}

void ANLGameMode::EndDay()
{
	bDayActive = false;
	UGameplayStatics::PlaySound2D(this, DayEndSound);
	OnDayEnded.Broadcast(CurrentDay);

	if (CurrentDay >= TotalDays)
	{
		UGameplayStatics::PlaySound2D(this, WinSound);
		OnGameWon.Broadcast();
		return;
	}

	const int32 NextDay = CurrentDay + 1;
	GetWorldTimerManager().SetTimer(TransitionTimer, [this, NextDay]() { StartDay(NextDay); }, DayTransitionDuration, false);
}

void ANLGameMode::TriggerGameOver(ENLGameOverReason Reason, float ScreenDelay)
{
	if (bGameOver) return;

	bGameOver = true;
	bDayActive = false;
	GetWorldTimerManager().ClearTimer(TransitionTimer);
	if (AmbientAudio) AmbientAudio->FadeOut(0.5f, 0.f);

	OnGameOver.Broadcast(Reason);

	GetWorldTimerManager().SetTimer(GameOverTimer, [this, Reason]() { ShowGameOverScreen(Reason); },
		FMath::Max(0.01f, ScreenDelay), false);
}

void ANLGameMode::ShowGameOverScreen(ENLGameOverReason Reason)
{
	UGameplayStatics::PlaySound2D(this, GameOverSound);
	OnShowGameOverScreen.Broadcast(Reason);
	GetWorldTimerManager().SetTimer(GameOverTimer, this, &ANLGameMode::RestartLevel, RestartDelay, false);
}

void ANLGameMode::RestartLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void ANLGameMode::RequestRunnerChase(ANLPlayerCharacter* Target)
{
	if (!Target || bGameOver) return;

	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
	{
		if (!It->IsChasing())
		{
			It->StartChase(Target);
			return;
		}
	}
}
