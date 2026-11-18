#include "NLGameMode.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "NLRunner.h"
#include "NLFuseBox.h"
#include "NLGenerator.h"
#include "NLAmbienceDirector.h"
#include "NLWeatherDirector.h"
#include "NLIntroDirector.h"
#include "NLGuideLanterns.h"
#include "NLGameSubsystem.h"
#include "NLSecurityDesk.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/WorldSettings.h"
#include "NavigationSystem.h"

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
	if (UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this)) Sub->ApplySettings(this);

	// Sunum için: oyunu "-NLDayLength=5" ile başlatınca günler 5 sn sürer
	float CmdDayLength = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("NLDayLength="), CmdDayLength) && CmdDayLength > 0.f)
	{
		DayLengthSeconds = CmdDayLength;
	}

	AmbientAudio = UGameplayStatics::SpawnSound2D(this, AmbientLoop);

	// Ses atmosferi: rüzgâr şiddeti, kıyı dalgaları, uzaktan sesler, iç mekân boğukluğu
	if (bSpawnAmbience && !TActorIterator<ANLAmbienceDirector>(GetWorld()))
	{
		GetWorld()->SpawnActor<ANLAmbienceDirector>(ANLAmbienceDirector::StaticClass(), FTransform::Identity);
	}
	// Hava durumu: yağmurlu gecelerde (varsayılan 1. gece) yağmur, şimşek, gök gürültüsü
	if (bSpawnWeather && !TActorIterator<ANLWeatherDirector>(GetWorld()))
	{
		GetWorld()->SpawnActor<ANLWeatherDirector>(ANLWeatherDirector::StaticClass(), FTransform::Identity);
	}

	// Dynamic NavMesh, önceden kurulmuş veri yoksa sadece oyun sırasında değişen yerleri örer (ada gibi büyük açık alanda Runner yol bulamaz).
	// Başta bir kez tamamını kur; "DAY 1" yazısı sırasında kısa bir bekleme olur.
	if (bBuildNavMeshOnStart)
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			const double T0 = FPlatformTime::Seconds();
			Nav->Build();
			UE_LOG(LogTemp, Log, TEXT("NLGame: NavMesh built in %.2f s"), FPlatformTime::Seconds() - T0);
		}
	}

	// Rıhtımdan fenere yol gösteren fenerler (NavMesh kurulduktan sonra)
	if (bGuideLanterns && !TActorIterator<ANLGuideLanterns>(GetWorld()))
	{
		ANLIntroDirector* Intro = TActorIterator<ANLIntroDirector>(GetWorld()) ? *TActorIterator<ANLIntroDirector>(GetWorld()) : nullptr;
		ANLSecurityDesk* Desk = TActorIterator<ANLSecurityDesk>(GetWorld()) ? *TActorIterator<ANLSecurityDesk>(GetWorld()) : nullptr;
		if (Intro && Desk)
		{
			if (ANLGuideLanterns* G = GetWorld()->SpawnActor<ANLGuideLanterns>(ANLGuideLanterns::StaticClass(), FTransform::Identity))
			{
				G->Build(Intro->GetActorLocation(), Desk->GetActorLocation());
			}
		}
	}

	// Diğer aktörler delegelere bağlansın diye 1 kare sonra: açılış sinematiği ya da doğrudan Gün 1
	GetWorldTimerManager().SetTimerForNextTick([this]() { BeginIntroOrDay(); });
}

void ANLGameMode::BeginIntroOrDay()
{
	ANLIntroDirector* Intro = TActorIterator<ANLIntroDirector>(GetWorld()) ? *TActorIterator<ANLIntroDirector>(GetWorld()) : nullptr;
	// Ölüp yeniden başlayınca (?NLNoIntro) sinematik yok, oyuncu rıhtımda başlar
	const bool bRestart = UGameplayStatics::HasOption(OptionsString, TEXT("NLNoIntro"));
	// Otomatik testler (-ExecCmds) ve -NLNoIntro: sinematik yok, oyuncu level'deki başlangıç noktasında kalır. -NLIntro ile zorla oynatılır.
	const TCHAR* Cmd = FCommandLine::Get();
	const bool bTestSkip = !FParse::Param(Cmd, TEXT("NLIntro")) && (FParse::Param(Cmd, TEXT("NLNoIntro")) || FCString::Stristr(Cmd, TEXT("ExecCmds=")) != nullptr);
	if (Intro && bPlayIntro && !bRestart && !bTestSkip)
	{
		Intro->Play();
		return;
	}
	// Devam / yeniden başlama: ?NLDay=N -> o geceden başla (1. gece rıhtımda, sonrakiler kontrol odasında)
	const int32 Day = FMath::Clamp(UGameplayStatics::GetIntOption(OptionsString, TEXT("NLDay"), 1), 1, TotalDays);
	if (Intro) Intro->PlaceDocked(bRestart && Day == 1);
	StartDay(Day);
}

void ANLGameMode::FinishIntro()
{
	if (CurrentDay == 0 && !bGameOver) StartDay(1);
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
	// Kayıt: her gece başında (otomatik testler kaydı ezmesin)
	if (!FCString::Stristr(FCommandLine::Get(), TEXT("ExecCmds=")))
	{
		if (UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this)) Sub->SaveProgress(CurrentDay, UGameplayStatics::GetCurrentLevelName(this));
	}
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
		if (UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this)) Sub->DeleteProgress();
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
	// Ölünce aynı gece baştan
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)), true, FString::Printf(TEXT("NLNoIntro?NLDay=%d"), FMath::Max(1, CurrentDay)));
}

void ANLGameMode::RequestRunnerChase(ANLPlayerCharacter* Target)
{
	if (!Target || bGameOver) return;

	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
	{
		if (!It->IsChasing())
		{
			It->PrepareChase(Target);
			return;
		}
	}
}

bool ANLGameMode::RequestColdRunner(ANLPlayerCharacter* Target)
{
	if (!Target || bGameOver) return false;
	ANLRunner* Free = nullptr;
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
	{
		if (It->IsChasing()) return true;   // zaten biri geliyor
		if (!Free) Free = *It;
	}
	if (!Free) return false;
	Free->PrepareColdHunt(Target, ColdRunnerDelay, 1500.f, 2800.f);
	return true;
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

void ANLGameMode::NLDebugRain(int32 bOn)
{
	for (TActorIterator<ANLWeatherDirector> It(GetWorld()); It; ++It) It->SetRaining(bOn != 0);
}

void ANLGameMode::NLDebugLightning()
{
	for (TActorIterator<ANLWeatherDirector> It(GetWorld()); It; ++It) It->TriggerLightning();
}
