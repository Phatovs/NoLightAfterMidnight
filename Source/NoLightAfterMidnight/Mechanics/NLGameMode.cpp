#include "NLGameMode.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "NLRunner.h"
#include "NLFuseBox.h"
#include "NLGenerator.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/WorldSettings.h"

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

	// Sunum için: oyunu "-NLDayLength=5" ile başlatınca günler 5 sn sürer
	float CmdDayLength = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("NLDayLength="), CmdDayLength) && CmdDayLength > 0.f)
	{
		DayLengthSeconds = CmdDayLength;
	}

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
	UE_LOG(LogTemp, Log, TEXT("NLGame: DAY %d started (%.0f s long)"), CurrentDay, DayLengthSeconds);
	OnDayStarted.Broadcast(CurrentDay);
}

void ANLGameMode::EndDay()
{
	bDayActive = false;
	UE_LOG(LogTemp, Log, TEXT("NLGame: DAY %d ended"), CurrentDay);
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

void ANLGameMode::NLDebugRunner()
{
	ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	UE_LOG(LogTemp, Log, TEXT("NLDebugRunner: player=%s"), *GetNameSafe(Player));
	RequestRunnerChase(Player);
}

void ANLGameMode::NLDebugTimeScale(float Scale)
{
	const float S = FMath::Clamp(Scale, 0.1f, 50.f);
	GetWorldSettings()->SetTimeDilation(S);
	UE_LOG(LogTemp, Log, TEXT("NLDebugTimeScale: %.1fx"), S);
}

void ANLGameMode::NLDebugFuseIn(float Seconds)
{
	for (TActorIterator<ANLFuseBox> It(GetWorld()); It; ++It)
	{
		It->SetTripTimer(FMath::Max(Seconds, 0.f));
		UE_LOG(LogTemp, Log, TEXT("NLDebugFuseIn: breaker will trip in %.1f s"), Seconds);
	}
}

void ANLGameMode::NLDebugDayLength(float Seconds)
{
	const float NewLength = FMath::Max(Seconds, 1.f);
	// Saat geri sarmasın: günün ilerleme oranı korunur
	const float Progress = GetDayProgress();
	DayLengthSeconds = NewLength;
	if (bDayActive) DayTimeRemaining = NewLength * (1.f - Progress);
	UE_LOG(LogTemp, Log, TEXT("NLDebugDayLength: days now last %.1f s"), NewLength);
}

void ANLGameMode::NLDebugEndDayIn(float Seconds)
{
	if (!bDayActive) return;
	DayTimeRemaining = FMath::Clamp(Seconds, 0.f, DayTimeRemaining);
	UE_LOG(LogTemp, Log, TEXT("NLDebugEndDayIn: day ends in %.1f s"), DayTimeRemaining);
}

void ANLGameMode::NLDebugGeneratorIn(float Seconds)
{
	for (TActorIterator<ANLGenerator> It(GetWorld()); It; ++It)
	{
		It->SetBreakdownTimer(FMath::Max(Seconds, 0.f));
		UE_LOG(LogTemp, Log, TEXT("NLDebugGeneratorIn: generator will break in %.1f s"), Seconds);
	}
}
