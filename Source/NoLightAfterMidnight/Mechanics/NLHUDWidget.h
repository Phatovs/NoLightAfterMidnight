// NoLightAfterMidnight - HUD: saat, gün, kamera görüntüsü, benzin, tamir barı, mesajlar
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Templates/Function.h"
#include "NLTypes.h"
#include "NLHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UWidget;
class UCanvasPanel;
class UTexture2D;

/**
 * Tüm arayüz C++'ta kurulur; editörde Widget Blueprint yapmak GEREKMEZ.
 * İsterseniz bu sınıftan bir WBP türetip aynı isimlerle kendi tasarımınızı yapabilirsiniz:
 * tasarım varsa kod kendi düzenini kurmaz, sizinkini kullanır.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Her zaman ---
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DayText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> InteractPromptText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> FlashlightBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FuelCanText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GeneratorStatusText;

	// --- Masada (kamera arayüzü) ---
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> CameraPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CameraFeedImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> NoPowerPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CameraNameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> FuelBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> FlashCooldownBar;
	// CCTV düzeni: sağ altta tarih/saat, sağ üstte kamera haritası, sol altta pil (jeneratör benzini), ortada nişangah
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FeedTimestampText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CameraMapText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FuelPercentText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> FlashReadyFill;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> FeedNoiseImage;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> FeedRollBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> FlashlightLabel;

	// --- Tamir mini oyunu ---
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RepairPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> RepairProgressBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RepairZone;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> RepairNeedle;

	// --- Ekran ortası ---
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CenterMessageText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> GameOverPanel;

	UPROPERTY(EditAnywhere, Category = "NL|HUD") float RepairTrackWidth = 400.f;
	UPROPERTY(EditAnywhere, Category = "NL|HUD") int32 NightStartHour = 0;
	UPROPERTY(EditAnywhere, Category = "NL|HUD") int32 NightEndHour = 6;
	UPROPERTY(EditAnywhere, Category = "NL|HUD") float CenterMessageDuration = 4.f;

	// Kamera kaydının sağ altındaki tarih: 1. gece bu tarih, her gece +1 gün
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") int32 FeedStartYear = 2025;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") int32 FeedStartMonth = 3;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") int32 FeedStartDay = 12;
	// Ekran karıncalanmasının yoğunluğu ve kare hızı
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") float NoiseOpacity = 0.14f;
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") float NoiseFPS = 24.f;
	// Kamera değişince çıkan parazit süresi
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") float SwitchStaticDuration = 0.35f;
	// Ekrandan aşağı kayan parlak bandın tur süresi (sn)
	UPROPERTY(EditAnywhere, Category = "NL|CCTV") float RollBarPeriod = 7.f;

	UFUNCTION() void HandleDayStarted(int32 Day);
	UFUNCTION() void HandleDayEnded(int32 Day);
	UFUNCTION() void HandleGameOverScreen(ENLGameOverReason Reason);
	UFUNCTION() void HandleGameWon();

private:
	float CenterMessageTimer = 0.f;
	bool bPersistentMessage = false;
	TWeakObjectPtr<UObject> BoundFeedTexture;

	// CCTV efektleri
	float NoiseTimer = 0.f;
	float RollBarY = -1.f;
	float SwitchStaticTimer = 0.f;
	int32 LastCameraIndex = INDEX_NONE;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> BatterySegments;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> ReticleParts;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> GeneratedTextures;

	void BuildDefaultLayout();
	void BuildCameraLayout(UCanvasPanel* Root);
	void TickCameraOverlay(const FGeometry& MyGeometry, float InDeltaTime, const class ANLSecurityDesk* Desk, const class ANLGenerator* Generator);
	UTexture2D* MakeProceduralTexture(int32 W, int32 H, TFunctionRef<FColor(int32, int32)> Pixel, bool bNearest);
	UImage* MakeRect(UCanvasPanel* Canvas, FName Name, FVector2D Position, FVector2D Size, FLinearColor Color);
	bool GetClock(int32& OutHour, int32& OutMinute) const;
	UTextBlock* MakeText(UCanvasPanel* Canvas, FName Name, const FString& Text, int32 Size, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FLinearColor Color = FLinearColor::White);
	UProgressBar* MakeBar(UCanvasPanel* Canvas, FName Name, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FVector2D Size, FLinearColor Fill);

	void ShowCenterMessage(const FString& Message, bool bPersistent);
	static void SetShown(UWidget* W, bool bShown);
};
