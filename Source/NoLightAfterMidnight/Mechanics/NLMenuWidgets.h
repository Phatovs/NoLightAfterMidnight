// NoLightAfterMidnight - Menüler (C++ ile kurulur, WBP gerekmez)
//  UNLMainMenuWidget  : NEW GAME / CONTINUE / SETTINGS / QUIT. Yazılar karanlıkta; fare (fener) üstüne gelince parlar.
//  UNLSettingsWidget  : ses, fare, görüş açısı, parlaklık, grafik kalitesi, pencere modu, çözünürlük, VSync
//  UNLPauseMenuWidget : oyunda ESC (ya da P): RESUME / SETTINGS / MAIN MENU / QUIT GAME
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NLMenuWidgets.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UButton;
class USlider;
class UBorder;

// Test: ekransız çalışırken fare yerine kullanılacak imleç (görüntü alanında 0..1)
namespace NLMenuDebug { extern NOLIGHTAFTERMIDNIGHT_API TOptional<FVector2D> Cursor; }

// Dinamik delegelere lambda bağlamak için küçük aracı
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLUIProxy : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> OnClickFn;
	TFunction<void()> OnHoverFn;
	TFunction<void(float)> OnValueFn;
	UFUNCTION() void HandleClick() { if (OnClickFn) OnClickFn(); }
	UFUNCTION() void HandleHover() { if (OnHoverFn) OnHoverFn(); }
	UFUNCTION() void HandleValue(float V) { if (OnValueFn) OnValueFn(V); }
};

UCLASS(Abstract)
class NOLIGHTAFTERMIDNIGHT_API UNLMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void Build(UCanvasPanel* Root) {}

	UTextBlock* NewText(const FString& Text, int32 Size, const FLinearColor& Color, const TCHAR* Typeface = TEXT("Regular"), int32 LetterSpacing = 0);
	UButton* NewButton(UWidget* Content, TFunction<void()> OnClick, bool bHoverSound = true);
	USlider* NewSlider(float Value, float Min, float Max, TFunction<void(float)> OnChanged);

	// Fenerle aydınlanan yazı: imlece yakınlığa göre karanlık -> sıcak beyaz
	void AddLitText(UTextBlock* T, float Radius = 230.f, float DimAlpha = 0.1f);
	TArray<TPair<TObjectPtr<UTextBlock>, FVector2D>> LitTexts;   // (yazı, (yarıçap, karanlık parlaklık))
	bool bLitByCursor = true;

	void PlayUISound(const TCHAR* Name, float Volume = 1.f);

	UPROPERTY(Transient) TArray<TObjectPtr<UNLUIProxy>> Proxies;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	float FlickerTime = 0.f;
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLSettingsWidget : public UNLMenuWidgetBase
{
	GENERATED_BODY()

public:
	TFunction<void()> OnBack;

protected:
	virtual void Build(UCanvasPanel* Root) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UVerticalBox* List = nullptr;
	void AddSliderRow(const FString& Label, float Value, float Min, float Max, TFunction<FString(float)> Format, TFunction<void(float)> OnChanged);
	void AddChoiceRow(const FString& Label, const TArray<FString>& Choices, int32 Index, TFunction<void(int32)> OnChanged);
	void Back();
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLMainMenuWidget : public UNLMenuWidgetBase
{
	GENERATED_BODY()

public:
	void DebugShowSettings(bool bShow) { ShowSettings(bShow); }

protected:
	virtual void Build(UCanvasPanel* Root) override;

private:
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> MainPanel;
	UPROPERTY(Transient) TObjectPtr<UNLSettingsWidget> Settings;
	bool bLeaving = false;
	void Leave(TFunction<void()> Then);
	void ShowSettings(bool bShow);
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLPauseMenuWidget : public UNLMenuWidgetBase
{
	GENERATED_BODY()

public:
	TFunction<void()> OnResume;

protected:
	virtual void Build(UCanvasPanel* Root) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> MainPanel;
	UPROPERTY(Transient) TObjectPtr<UNLSettingsWidget> Settings;
	void ShowSettings(bool bShow);
};
