// NoLightAfterMidnight - Oyun geneli: ayarlar, kayıt (devam), level geçişleri
// Ayarlar "NLSettings" kayıt yuvasında; grafik ayarları motorun GameUserSettings'inde (kendisi kaydeder).
// Kayıt: her gece başladığında o gecenin numarası "NLProgress" yuvasına yazılır; ana menüde Continue oradan açar.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NLGameSubsystem.generated.h"

class USoundMix;
class USoundClass;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float AmbienceVolume = 1.f;
	// Oyuncunun kendi fare hassasiyetiyle çarpılır
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() bool bInvertY = false;
	UPROPERTY() float FieldOfView = 90.f;
	// Ekran gaması (parlaklık); motor varsayılanı 2.2
	UPROPERTY() float Gamma = 2.2f;
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLProgressSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 Night = 1;
	UPROPERTY() FString Level;
	UPROPERTY() FDateTime SavedAt;
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UNLGameSubsystem* Get(const UObject* WorldContext);

	// --- Levellar ---
	FString GameLevel = TEXT("/Game/Levels/Lvl_Ada_Art");
	FString MenuLevel = TEXT("/Game/Levels/Lvl_MainMenu");

	void StartNewGame(const UObject* WorldContext);
	void ContinueGame(const UObject* WorldContext);
	void GoToMainMenu(const UObject* WorldContext);
	void QuitGame(const UObject* WorldContext);

	// --- Kayıt ---
	bool HasProgress() const;
	int32 GetSavedNight() const;
	void SaveProgress(int32 Night, const FString& Level);
	void DeleteProgress();

	// --- Ayarlar ---
	UNLSettingsSave* GetSettings() const { return Settings; }
	void SaveSettings();
	// Ses (ana ses), parlaklık: her level açılışında ve ayar değişince
	void ApplySettings(const UObject* WorldContext);

	float GetAmbienceVolume() const { return Settings ? Settings->AmbienceVolume : 1.f; }

	// Grafik (motorun GameUserSettings'i)
	int32 GetQuality() const;
	void SetQuality(int32 Level);
	int32 GetWindowMode() const;            // 0 tam ekran, 1 pencereli tam ekran, 2 pencere
	void SetWindowMode(int32 Mode);
	FIntPoint GetResolution() const;
	void SetResolution(FIntPoint Res);
	bool GetVSync() const;
	void SetVSync(bool bOn);
	static TArray<FIntPoint> GetResolutionChoices();

private:
	UPROPERTY(Transient) TObjectPtr<UNLSettingsSave> Settings;
	UPROPERTY(Transient) TObjectPtr<USoundMix> VolumeMix;
	UPROPERTY(Transient) TObjectPtr<USoundClass> MasterClass;
};
