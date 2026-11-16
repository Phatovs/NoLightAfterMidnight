#include "NLMenuWidgets.h"
#include "NLGameSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"

TOptional<FVector2D> NLMenuDebug::Cursor;

namespace NLUI
{
	const FLinearColor Lit(1.f, 0.9f, 0.74f, 1.f);
	const FLinearColor Text(0.78f, 0.76f, 0.72f, 1.f);
	const FLinearColor Faint(0.45f, 0.44f, 0.42f, 1.f);

	UCanvasPanelSlot* Place(UCanvasPanel* C, UWidget* W, FVector2D Anchor, FVector2D Align, FVector2D Pos)
	{
		UCanvasPanelSlot* S = C->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		S->SetAlignment(Align);
		S->SetPosition(Pos);
		S->SetAutoSize(true);
		return S;
	}

	UCanvasPanelSlot* Fill(UCanvasPanel* C, UWidget* W)
	{
		UCanvasPanelSlot* S = C->AddChildToCanvas(W);
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
		return S;
	}
}

// ------------------------------------------------------------------ temel

TSharedRef<SWidget> UNLMenuWidgetBase::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = RootCanvas;
		Build(RootCanvas);
	}
	return Super::RebuildWidget();
}

UTextBlock* UNLMenuWidgetBase::NewText(const FString& Text, int32 Size, const FLinearColor& Color, const TCHAR* Typeface, int32 LetterSpacing)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Text));
	FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
	// Menü fontu (Content/UI/FontSources/*.ttf, oyun açılırken yüklenir). -NLMenuFont=<dosya adı> ile denenebilir.
	static TSharedPtr<const FCompositeFont> MenuFont;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		FString File = TEXT("SpecialElite-Regular.ttf");
		FParse::Value(FCommandLine::Get(), TEXT("NLMenuFont="), File);
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("UI/FontSources") / File);
		if (!File.Equals(TEXT("Roboto"), ESearchCase::IgnoreCase) && FPaths::FileExists(Path))
		{
			MenuFont = MakeShared<FStandaloneCompositeFont>(NAME_None, Path, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			UE_LOG(LogTemp, Log, TEXT("NLMenu: font %s"), *File);
		}
	}
	if (MenuFont.IsValid())
	{
		// Bu fontlar Roboto'dan biraz küçük durur
		F = FSlateFontInfo(MenuFont, FMath::RoundToInt(Size * 1.12f));
	}
	F.LetterSpacing = LetterSpacing;
	T->SetFont(F);
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetShadowOffset(FVector2D(1.5f, 1.5f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	return T;
}

UButton* UNLMenuWidgetBase::NewButton(UWidget* Content, TFunction<void()> OnClick, bool bHoverSound)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style;
	Style.Normal = FSlateNoResource();
	Style.Hovered = FSlateNoResource();
	Style.Pressed = FSlateNoResource();
	Style.Disabled = FSlateNoResource();
	Style.NormalPadding = FMargin(2.f, 3.f);
	Style.PressedPadding = FMargin(2.f, 4.f, 2.f, 2.f);
	B->SetStyle(Style);
	B->SetBackgroundColor(FLinearColor::Transparent);
	B->AddChild(Content);
	UNLUIProxy* P = NewObject<UNLUIProxy>(this);
	P->OnClickFn = [this, OnClick]() { PlayUISound(TEXT("flashlight_click"), 0.8f); if (OnClick) OnClick(); };
	B->OnClicked.AddDynamic(P, &UNLUIProxy::HandleClick);
	if (bHoverSound)
	{
		P->OnHoverFn = [this]() { PlayUISound(TEXT("camera_switch"), 0.18f); };
		B->OnHovered.AddDynamic(P, &UNLUIProxy::HandleHover);
	}
	Proxies.Add(P);
	return B;
}

USlider* UNLMenuWidgetBase::NewSlider(float Value, float Min, float Max, TFunction<void(float)> OnChanged)
{
	USlider* S = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	S->SetMinValue(Min);
	S->SetMaxValue(Max);
	S->SetValue(Value);
	S->SetSliderBarColor(FLinearColor(0.25f, 0.24f, 0.22f));
	S->SetSliderHandleColor(NLUI::Lit);
	UNLUIProxy* P = NewObject<UNLUIProxy>(this);
	P->OnValueFn = OnChanged;
	S->OnValueChanged.AddDynamic(P, &UNLUIProxy::HandleValue);
	Proxies.Add(P);
	return S;
}

void UNLMenuWidgetBase::AddLitText(UTextBlock* T, float Radius, float DimAlpha)
{
	LitTexts.Add(TPair<TObjectPtr<UTextBlock>, FVector2D>(T, FVector2D(Radius, DimAlpha)));
}

void UNLMenuWidgetBase::PlayUISound(const TCHAR* Name, float Volume)
{
	const FString Path = FString::Printf(TEXT("/Game/Audio/NL/%s.%s"), Name, Name);
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) UGameplayStatics::PlaySound2D(this, S, Volume);
}

void UNLMenuWidgetBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bLitByCursor || !FSlateApplication::IsInitialized()) return;
	FlickerTime += InDeltaTime;
	// Fener ara ara hafifçe titrer
	const float Flicker = 0.93f + 0.07f * FMath::Sin(FlickerTime * 23.f) * FMath::Sin(FlickerTime * 3.1f);
	const FVector2D MousePos = NLMenuDebug::Cursor.IsSet() ? MyGeometry.LocalToAbsolute(MyGeometry.GetLocalSize() * NLMenuDebug::Cursor.GetValue()) : FSlateApplication::Get().GetCursorPos();
	const float Dpi = FMath::Max(MyGeometry.Scale, 0.5f);
	for (const TPair<TObjectPtr<UTextBlock>, FVector2D>& E : LitTexts)
	{
		UTextBlock* T = E.Key;
		if (!T || !T->IsVisible()) continue;
		const FGeometry& G = T->GetCachedGeometry();
		const FVector2D Center = G.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
		const FVector2D Half = G.GetAbsoluteSize() * 0.5f;
		// Yazının kutusuna olan uzaklık (uzun yazının kenarı da aydınlansın)
		const FVector2D D(FMath::Max(0.f, FMath::Abs(MousePos.X - Center.X) - Half.X), FMath::Max(0.f, FMath::Abs(MousePos.Y - Center.Y) - Half.Y));
		const float Dist = static_cast<float>(D.Size()) / Dpi;
		const float L = (1.f - FMath::SmoothStep(0.f, static_cast<float>(E.Value.X), Dist)) * Flicker;
		const float Dim = static_cast<float>(T->GetIsEnabled() ? E.Value.Y : E.Value.Y * 0.5);
		FLinearColor C = FMath::Lerp(FLinearColor(0.55f, 0.6f, 0.7f, Dim), NLUI::Lit, L);
		if (!T->GetIsEnabled()) C.A *= 0.45f;
		T->SetColorAndOpacity(FSlateColor(C));
	}
}

// ------------------------------------------------------------------ ayarlar

void UNLSettingsWidget::Build(UCanvasPanel* Root)
{
	using namespace NLUI;
	SetIsFocusable(true);
	bLitByCursor = false;

	UImage* Shade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Shade->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Fill(Root, Shade);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrush(FSlateColorBrush(FLinearColor(0.012f, 0.013f, 0.016f, 0.95f)));
	Panel->SetPadding(FMargin(48.f, 36.f));
	UCanvasPanelSlot* PS = Place(Root, Panel, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	PS->SetAutoSize(true);

	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(List);
	UTextBlock* Title = NewText(TEXT("SETTINGS"), 30, Lit, TEXT("Light"), 400);
	List->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));

	UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this);
	UNLSettingsSave* S = Sub ? Sub->GetSettings() : nullptr;
	if (!S) return;
	auto Apply = [this]() { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->ApplySettings(this); };
	auto Pct = [](float V) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f)); };

	AddSliderRow(TEXT("Master Volume"), S->MasterVolume, 0.f, 1.f, Pct, [S, Apply](float V) { S->MasterVolume = V; Apply(); });
	AddSliderRow(TEXT("Ambience Volume"), S->AmbienceVolume, 0.f, 1.f, Pct, [S](float V) { S->AmbienceVolume = V; });
	AddSliderRow(TEXT("Mouse Sensitivity"), S->MouseSensitivity, 0.2f, 2.5f, [](float V) { return FString::Printf(TEXT("%.2f"), V); }, [S](float V) { S->MouseSensitivity = V; });
	AddChoiceRow(TEXT("Invert Mouse Y"), { TEXT("Off"), TEXT("On") }, S->bInvertY ? 1 : 0, [S](int32 I) { S->bInvertY = I == 1; });
	AddSliderRow(TEXT("Field of View"), S->FieldOfView, 70.f, 110.f, [](float V) { return FString::Printf(TEXT("%d"), FMath::RoundToInt(V)); }, [S](float V) { S->FieldOfView = V; });
	AddSliderRow(TEXT("Brightness"), S->Gamma, 1.6f, 3.0f, [](float V) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt((V - 1.6f) / 1.4f * 100.f)); }, [S, Apply](float V) { S->Gamma = V; Apply(); });
	AddChoiceRow(TEXT("Graphics Quality"), { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") }, Sub->GetQuality(), [this](int32 I) { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->SetQuality(I); });
	AddChoiceRow(TEXT("Window Mode"), { TEXT("Fullscreen"), TEXT("Borderless"), TEXT("Windowed") }, Sub->GetWindowMode(), [this](int32 I) { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->SetWindowMode(I); });
	{
		const TArray<FIntPoint> Res = UNLGameSubsystem::GetResolutionChoices();
		TArray<FString> Names;
		int32 Cur = 2;
		for (int32 i = 0; i < Res.Num(); ++i)
		{
			Names.Add(FString::Printf(TEXT("%d x %d"), Res[i].X, Res[i].Y));
			if (Res[i] == Sub->GetResolution()) Cur = i;
		}
		AddChoiceRow(TEXT("Resolution"), Names, Cur, [this, Res](int32 I) { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->SetResolution(Res[I]); });
	}
	AddChoiceRow(TEXT("VSync"), { TEXT("Off"), TEXT("On") }, Sub->GetVSync() ? 1 : 0, [this](int32 I) { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->SetVSync(I == 1); });

	UTextBlock* BackText = NewText(TEXT("BACK"), 22, Lit, TEXT("Regular"), 250);
	UButton* BackB = NewButton(BackText, [this]() { Back(); });
	List->AddChildToVerticalBox(BackB)->SetPadding(FMargin(0.f, 26.f, 0.f, 0.f));
}

void UNLSettingsWidget::AddSliderRow(const FString& Label, float Value, float Min, float Max, TFunction<FString(float)> Format, TFunction<void(float)> OnChanged)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* LBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LBox->SetWidthOverride(280.f);
	LBox->AddChild(NewText(Label, 17, NLUI::Text));
	Row->AddChildToHorizontalBox(LBox)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* ValueText = NewText(Format(Value), 17, NLUI::Lit);
	USizeBox* SBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SBox->SetWidthOverride(300.f);
	SBox->AddChild(NewSlider(Value, Min, Max, [ValueText, Format, OnChanged](float V) { ValueText->SetText(FText::FromString(Format(V))); OnChanged(V); }));
	Row->AddChildToHorizontalBox(SBox)->SetVerticalAlignment(VAlign_Center);
	Row->AddChildToHorizontalBox(ValueText)->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));
	List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 6.f));
}

void UNLSettingsWidget::AddChoiceRow(const FString& Label, const TArray<FString>& Choices, int32 Index, TFunction<void(int32)> OnChanged)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* LBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LBox->SetWidthOverride(280.f);
	LBox->AddChild(NewText(Label, 17, NLUI::Text));
	Row->AddChildToHorizontalBox(LBox)->SetVerticalAlignment(VAlign_Center);

	TSharedRef<int32> Cur = MakeShared<int32>(FMath::Clamp(Index, 0, Choices.Num() - 1));
	UTextBlock* ValueText = NewText(Choices.IsValidIndex(*Cur) ? Choices[*Cur] : FString(), 17, NLUI::Lit);
	ValueText->SetJustification(ETextJustify::Center);
	auto Step = [Cur, Choices, ValueText, OnChanged](int32 D)
	{
		*Cur = (*Cur + D + Choices.Num()) % Choices.Num();
		ValueText->SetText(FText::FromString(Choices[*Cur]));
		OnChanged(*Cur);
	};
	Row->AddChildToHorizontalBox(NewButton(NewText(TEXT("<"), 18, NLUI::Text), [Step]() { Step(-1); }, false))->SetVerticalAlignment(VAlign_Center);
	USizeBox* VBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	VBox->SetWidthOverride(230.f);
	VBox->AddChild(ValueText);
	Row->AddChildToHorizontalBox(VBox)->SetVerticalAlignment(VAlign_Center);
	Row->AddChildToHorizontalBox(NewButton(NewText(TEXT(">"), 18, NLUI::Text), [Step]() { Step(1); }, false))->SetVerticalAlignment(VAlign_Center);
	List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
}

void UNLSettingsWidget::Back()
{
	if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this))
	{
		G->ApplySettings(this);
		G->SaveSettings();
	}
	if (OnBack) OnBack();
}

FReply UNLSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::P)
	{
		Back();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ------------------------------------------------------------------ ana menü

void UNLMainMenuWidget::Build(UCanvasPanel* Root)
{
	using namespace NLUI;
	SetIsFocusable(true);
	MainPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Main"));
	Fill(Root, MainPanel);

	UTextBlock* T1 = NewText(TEXT("NO LIGHT"), 64, Lit, TEXT("Light"), 500);
	UTextBlock* T2 = NewText(TEXT("AFTER MIDNIGHT"), 30, Lit, TEXT("Light"), 900);
	UVerticalBox* TitleBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TitleBox->AddChildToVerticalBox(T1);
	TitleBox->AddChildToVerticalBox(T2)->SetPadding(FMargin(4.f, -6.f, 0.f, 0.f));
	Place(MainPanel, TitleBox, FVector2D(0.08f, 0.14f), FVector2D(0.f, 0.f), FVector2D::ZeroVector);
	AddLitText(T1, 420.f, 0.22f);
	AddLitText(T2, 420.f, 0.2f);

	UVerticalBox* Items = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Place(MainPanel, Items, FVector2D(0.08f, 0.56f), FVector2D(0.f, 0.5f), FVector2D::ZeroVector);

	UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this);
	const bool bHasSave = Sub && Sub->HasProgress();
	auto AddItem = [&](const FString& Label, TFunction<void()> OnClick, bool bEnabled, const FString& Sub2)
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* T = NewText(Label, 30, Lit, TEXT("Regular"), 350);
		T->SetIsEnabled(bEnabled);
		Box->AddChildToVerticalBox(T);
		AddLitText(T, 240.f, 0.3f);
		if (!Sub2.IsEmpty())
		{
			UTextBlock* S2 = NewText(Sub2, 14, Faint, TEXT("Italic"), 200);
			S2->SetIsEnabled(bEnabled);
			Box->AddChildToVerticalBox(S2)->SetPadding(FMargin(3.f, -2.f, 0.f, 0.f));
			AddLitText(S2, 200.f, 0.22f);
		}
		UButton* B = NewButton(Box, OnClick);
		B->SetIsEnabled(bEnabled);
		Items->AddChildToVerticalBox(B)->SetPadding(FMargin(0.f, 10.f));
	};
	AddItem(TEXT("NEW GAME"), [this]() { Leave([this]() { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->StartNewGame(this); }); }, true, FString());
	AddItem(TEXT("CONTINUE"), [this]() { Leave([this]() { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->ContinueGame(this); }); },
		bHasSave, bHasSave ? FString::Printf(TEXT("Night %d"), Sub->GetSavedNight()) : FString(TEXT("No saved night")));
	AddItem(TEXT("SETTINGS"), [this]() { ShowSettings(true); }, true, FString());
	AddItem(TEXT("QUIT"), [this]() { Leave([this]() { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->QuitGame(this); }); }, true, FString());


	Settings = CreateWidget<UNLSettingsWidget>(GetOwningPlayer(), UNLSettingsWidget::StaticClass());
	if (Settings)
	{
		Fill(Root, Settings);
		Settings->OnBack = [this]() { ShowSettings(false); };
		Settings->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNLMainMenuWidget::ShowSettings(bool bShow)
{
	if (Settings) Settings->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MainPanel->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (bShow && Settings) Settings->SetKeyboardFocus(); else SetKeyboardFocus();
}

void UNLMainMenuWidget::Leave(TFunction<void()> Then)
{
	if (bLeaving) return;
	bLeaving = true;
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.9f, FLinearColor::Black, true, true);
	}
	FTimerHandle H;
	GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Then]() { Then(); }), 1.f, false);
}

// ------------------------------------------------------------------ duraklatma

void UNLPauseMenuWidget::Build(UCanvasPanel* Root)
{
	using namespace NLUI;
	SetIsFocusable(true);
	UImage* Shade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Shade->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.72f));
	Fill(Root, Shade);

	MainPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Main"));
	Fill(Root, MainPanel);
	UVerticalBox* Items = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Place(MainPanel, Items, FVector2D(0.08f, 0.5f), FVector2D(0.f, 0.5f), FVector2D::ZeroVector);
	UTextBlock* Title = NewText(TEXT("PAUSED"), 44, Lit, TEXT("Light"), 500);
	Items->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 30.f));
	AddLitText(Title, 400.f, 0.35f);

	auto AddItem = [&](const FString& Label, TFunction<void()> OnClick)
	{
		UTextBlock* T = NewText(Label, 26, Lit, TEXT("Regular"), 300);
		AddLitText(T, 220.f, 0.4f);
		Items->AddChildToVerticalBox(NewButton(T, OnClick))->SetPadding(FMargin(0.f, 8.f));
	};
	AddItem(TEXT("RESUME"), [this]() { if (OnResume) OnResume(); });
	AddItem(TEXT("SETTINGS"), [this]() { ShowSettings(true); });
	AddItem(TEXT("MAIN MENU"), [this]() { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->GoToMainMenu(this); });
	AddItem(TEXT("QUIT GAME"), [this]() { if (UNLGameSubsystem* G = UNLGameSubsystem::Get(this)) G->QuitGame(this); });

	UTextBlock* Hint = NewText(TEXT("Progress is saved at the start of each night."), 13, Faint, TEXT("Italic"), 100);
	Place(MainPanel, Hint, FVector2D(0.08f, 0.92f), FVector2D(0.f, 1.f), FVector2D::ZeroVector);

	Settings = CreateWidget<UNLSettingsWidget>(GetOwningPlayer(), UNLSettingsWidget::StaticClass());
	if (Settings)
	{
		Fill(Root, Settings);
		Settings->OnBack = [this]() { ShowSettings(false); };
		Settings->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNLPauseMenuWidget::ShowSettings(bool bShow)
{
	if (Settings) Settings->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	MainPanel->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (bShow && Settings) Settings->SetKeyboardFocus(); else SetKeyboardFocus();
}

FReply UNLPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::P)
	{
		if (Settings && Settings->IsVisible()) ShowSettings(false);
		else if (OnResume) OnResume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
