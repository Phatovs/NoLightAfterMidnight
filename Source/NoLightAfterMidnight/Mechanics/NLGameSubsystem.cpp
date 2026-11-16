#include "NLGameSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace NLSave
{
	static const TCHAR* SettingsSlot = TEXT("NLSettings");
	static const TCHAR* ProgressSlot = TEXT("NLProgress");
}

void UNLGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Settings = Cast<UNLSettingsSave>(UGameplayStatics::LoadGameFromSlot(NLSave::SettingsSlot, 0));
	if (!Settings) Settings = Cast<UNLSettingsSave>(UGameplayStatics::CreateSaveGameObject(UNLSettingsSave::StaticClass()));
	MasterClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Master.Master"));
	VolumeMix = NewObject<USoundMix>(this, TEXT("NL_VolumeMix"));
	if (GEngine) GEngine->DisplayGamma = Settings->Gamma;
}

UNLGameSubsystem* UNLGameSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNLGameSubsystem>() : nullptr;
}

// ------------------------------------------------------------------ level geçişleri

void UNLGameSubsystem::StartNewGame(const UObject* WorldContext)
{
	// Yeni oyun: rıhtım sinematiğiyle 1. gece (kayıt gece başlayınca yazılır)
	UGameplayStatics::OpenLevel(WorldContext, FName(*GameLevel));
}

void UNLGameSubsystem::ContinueGame(const UObject* WorldContext)
{
	if (!HasProgress()) { StartNewGame(WorldContext); return; }
	const UNLProgressSave* P = Cast<UNLProgressSave>(UGameplayStatics::LoadGameFromSlot(NLSave::ProgressSlot, 0));
	const FString Level = P && !P->Level.IsEmpty() ? P->Level : GameLevel;
	UGameplayStatics::OpenLevel(WorldContext, FName(*Level), true, FString::Printf(TEXT("NLNoIntro?NLDay=%d"), GetSavedNight()));
}

void UNLGameSubsystem::GoToMainMenu(const UObject* WorldContext)
{
	UGameplayStatics::SetGamePaused(WorldContext, false);
	UGameplayStatics::OpenLevel(WorldContext, FName(*MenuLevel));
}

void UNLGameSubsystem::QuitGame(const UObject* WorldContext)
{
	SaveSettings();
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	UKismetSystemLibrary::QuitGame(W, W ? W->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
}

// ------------------------------------------------------------------ kayıt

bool UNLGameSubsystem::HasProgress() const
{
	return UGameplayStatics::DoesSaveGameExist(NLSave::ProgressSlot, 0);
}

int32 UNLGameSubsystem::GetSavedNight() const
{
	const UNLProgressSave* P = Cast<UNLProgressSave>(UGameplayStatics::LoadGameFromSlot(NLSave::ProgressSlot, 0));
	return P ? FMath::Max(1, P->Night) : 1;
}

void UNLGameSubsystem::SaveProgress(int32 Night, const FString& Level)
{
	UNLProgressSave* P = Cast<UNLProgressSave>(UGameplayStatics::CreateSaveGameObject(UNLProgressSave::StaticClass()));
	P->Night = Night;
	P->Level = Level;
	P->SavedAt = FDateTime::Now();
	UGameplayStatics::SaveGameToSlot(P, NLSave::ProgressSlot, 0);
	UE_LOG(LogTemp, Log, TEXT("NLSave: progress saved (night %d, %s)"), Night, *Level);
}

void UNLGameSubsystem::DeleteProgress()
{
	UGameplayStatics::DeleteGameInSlot(NLSave::ProgressSlot, 0);
}

// ------------------------------------------------------------------ ayarlar

void UNLGameSubsystem::SaveSettings()
{
	if (Settings) UGameplayStatics::SaveGameToSlot(Settings, NLSave::SettingsSlot, 0);
	if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr) G->SaveSettings();
}

void UNLGameSubsystem::ApplySettings(const UObject* WorldContext)
{
	if (!Settings) return;
	if (GEngine) GEngine->DisplayGamma = Settings->Gamma;
	if (VolumeMix && MasterClass && WorldContext)
	{
		UGameplayStatics::SetSoundMixClassOverride(WorldContext, VolumeMix, MasterClass, FMath::Clamp(Settings->MasterVolume, 0.f, 1.f), 1.f, 0.f, true);
		UGameplayStatics::PushSoundMixModifier(WorldContext, VolumeMix);
	}
}

int32 UNLGameSubsystem::GetQuality() const
{
	const UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const int32 Q = G ? G->GetOverallScalabilityLevel() : 2;
	return Q < 0 ? 2 : FMath::Clamp(Q, 0, 3);
}

void UNLGameSubsystem::SetQuality(int32 Level)
{
	if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		G->SetOverallScalabilityLevel(FMath::Clamp(Level, 0, 3));
		G->ApplySettings(false);
	}
}

int32 UNLGameSubsystem::GetWindowMode() const
{
	const UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return G ? static_cast<int32>(G->GetFullscreenMode()) : 1;
}

void UNLGameSubsystem::SetWindowMode(int32 Mode)
{
	if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		G->SetFullscreenMode(static_cast<EWindowMode::Type>(FMath::Clamp(Mode, 0, 2)));
		G->ApplySettings(false);
	}
}

FIntPoint UNLGameSubsystem::GetResolution() const
{
	const UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return G ? G->GetScreenResolution() : FIntPoint(1920, 1080);
}

void UNLGameSubsystem::SetResolution(FIntPoint Res)
{
	if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		G->SetScreenResolution(Res);
		G->ApplySettings(false);
	}
}

bool UNLGameSubsystem::GetVSync() const
{
	const UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return G && G->IsVSyncEnabled();
}

void UNLGameSubsystem::SetVSync(bool bOn)
{
	if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		G->SetVSyncEnabled(bOn);
		G->ApplySettings(false);
	}
}

TArray<FIntPoint> UNLGameSubsystem::GetResolutionChoices()
{
	return { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
}
