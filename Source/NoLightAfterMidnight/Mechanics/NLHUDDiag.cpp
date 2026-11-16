// NoLightAfterMidnight - HUD: güvenlik masasındaki POWER DIAGNOSTICS paneli
// Arızanın ne olduğu (jeneratör arızası / benzin bitti / sigorta kablolaması) ve sigortaların doğru sırası
// SADECE masadaki monitörde görünür. Dışarıda ekranda hiçbir şey yazmaz; oyuncu sesten anlar.
// Panel yedek aküyle çalışır: elektrik yokken de (NO SIGNAL ekranında) görünür.
#include "NLHUDWidget.h"
#include "NLPlayerCharacter.h"
#include "NLPowerSubsystem.h"
#include "NLGenerator.h"
#include "NLFuseBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

namespace NLDiag
{
	FSlateFontInfo Mono(int32 Size)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size);
		Font.LetterSpacing = 60;
		return Font;
	}

	const FLinearColor Ok(0.62f, 0.85f, 0.62f, 1.f);
	const FLinearColor Bad(1.f, 0.32f, 0.26f, 1.f);
	const FLinearColor Warn(1.f, 0.8f, 0.35f, 1.f);
	const FLinearColor Label(0.72f, 0.72f, 0.72f, 1.f);
}

void UNLHUDWidget::BuildDiagPanel(UCanvasPanel* Root)
{
	using namespace NLDiag;
	DiagPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DiagPanel"));
	UCanvasPanelSlot* S = Root->AddChildToCanvas(DiagPanel);
	S->SetAnchors(FAnchors(0.f, 0.5f));
	S->SetAlignment(FVector2D(0.f, 0.5f));
	S->SetPosition(FVector2D(40.f, 10.f));
	S->SetSize(FVector2D(DiagWidth, 250.f));

	MakeRect(DiagPanel, TEXT("DiagBg"), FVector2D(0, 0), FVector2D(DiagWidth, 250.f), FLinearColor(0.f, 0.f, 0.f, 0.62f));
	MakeRect(DiagPanel, TEXT("DiagEdge"), FVector2D(0, 0), FVector2D(3.f, 250.f), FLinearColor(1.f, 1.f, 1.f, 0.35f));

	auto Line = [this](FName Name, const FString& Text, int32 Size, FVector2D Pos, const FLinearColor& C) -> UTextBlock*
	{
		UTextBlock* T = MakeText(DiagPanel, Name, Text, Size, FVector2D(0, 0), FVector2D(0, 0), Pos, C);
		T->SetFont(Mono(Size));
		return T;
	};
	Line(TEXT("DiagTitle"), TEXT("POWER DIAGNOSTICS"), 15, FVector2D(16, 10), FLinearColor(0.92f, 0.92f, 0.92f, 1.f));
	DiagGenText  = Line(TEXT("DiagGen"),  TEXT("GENERATOR  --"), 13, FVector2D(16, 40), Label);
	DiagFuelText = Line(TEXT("DiagFuel"), TEXT("FUEL       --"), 13, FVector2D(16, 62), Label);
	DiagFuseText = Line(TEXT("DiagFuse"), TEXT("FUSE BOX   --"), 13, FVector2D(16, 84), Label);
	Line(TEXT("DiagOrderTitle"), TEXT("FUSE WIRING ORDER"), 12, FVector2D(16, 116), Label);

	// 2x2: sol üst, sağ üst, sol alt, sağ alt (kutuya karşıdan bakınca)
	DiagSwatches.Reset();
	DiagSlotTexts.Reset();
	for (int32 i = 0; i < ANLFuseBox::NumFuses; ++i)
	{
		const float X = 16.f + (i % 2) * 150.f;
		const float Y = 140.f + (i / 2) * 44.f;
		MakeRect(DiagPanel, *FString::Printf(TEXT("DiagSlotFrame%d"), i), FVector2D(X, Y), FVector2D(140.f, 36.f), FLinearColor(1.f, 1.f, 1.f, 0.18f));
		DiagSwatches.Add(MakeRect(DiagPanel, *FString::Printf(TEXT("DiagSlotColor%d"), i), FVector2D(X + 4.f, Y + 4.f), FVector2D(28.f, 28.f), FLinearColor::White));
		DiagSlotTexts.Add(Line(*FString::Printf(TEXT("DiagSlotName%d"), i), TEXT("--"), 13, FVector2D(X + 40.f, Y + 9.f), FLinearColor::White));
	}
	SetShown(DiagPanel, false);
}

void UNLHUDWidget::TickDiagPanel(float InDeltaTime, bool bAtMonitor, const UNLPowerSubsystem* Power)
{
	using namespace NLDiag;
	SetShown(DiagPanel, bAtMonitor && Power != nullptr);
	if (!bAtMonitor || !Power) return;

	const ANLGenerator* Gen = Power->GetGenerator();
	const ANLFuseBox* Fuse = Power->GetFuseBox();
	const bool bBlink = FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 1.f) < 0.6f;

	auto Set = [bBlink](UTextBlock* T, const FString& Text, const FLinearColor& C, bool bAlarm)
	{
		if (!T) return;
		T->SetText(FText::FromString(Text));
		FLinearColor Col = C;
		if (bAlarm && !bBlink) Col.A = 0.35f;
		T->SetColorAndOpacity(FSlateColor(Col));
	};

	if (Gen)
	{
		const float Fuel = Gen->GetFuelPercent();
		if (Gen->IsBroken())       Set(DiagGenText, TEXT("GENERATOR  FAULT - REPAIR ON SITE"), Bad, true);
		else if (Fuel <= 0.f)      Set(DiagGenText, TEXT("GENERATOR  STOPPED - NO FUEL"), Bad, true);
		else                       Set(DiagGenText, TEXT("GENERATOR  RUNNING"), Ok, false);

		const int32 Pct = FMath::RoundToInt(Fuel * 100.f);
		if (Fuel <= 0.f)           Set(DiagFuelText, TEXT("FUEL       EMPTY - REFUEL"), Bad, true);
		else if (Fuel < 0.2f)      Set(DiagFuelText, FString::Printf(TEXT("FUEL       %d%% - LOW"), Pct), Warn, true);
		else                       Set(DiagFuelText, FString::Printf(TEXT("FUEL       %d%%"), Pct), Ok, false);
	}
	else
	{
		Set(DiagGenText, TEXT("GENERATOR  NO DATA"), Label, false);
		Set(DiagFuelText, TEXT("FUEL       NO DATA"), Label, false);
	}

	if (Fuse)
	{
		if (Fuse->IsTripped())     Set(DiagFuseText, TEXT("FUSE BOX   WIRING MISMATCH"), Bad, true);
		else if (!Fuse->IsOn())    Set(DiagFuseText, TEXT("FUSE BOX   SWITCH OFF"), Warn, true);
		else                       Set(DiagFuseText, TEXT("FUSE BOX   OK"), Ok, false);
	}
	else
	{
		Set(DiagFuseText, TEXT("FUSE BOX   NO DATA"), Label, false);
	}

	for (int32 i = 0; i < DiagSwatches.Num(); ++i)
	{
		const int32 Want = Fuse ? Fuse->GetTargetFuse(i) : i;
		if (DiagSwatches[i]) DiagSwatches[i]->SetColorAndOpacity(ANLFuseBox::GetFuseColor(Want));
		if (DiagSlotTexts.IsValidIndex(i) && DiagSlotTexts[i])
		{
			static const TCHAR* Pos[ANLFuseBox::NumFuses] = { TEXT("TL"), TEXT("TR"), TEXT("BL"), TEXT("BR") };
			DiagSlotTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("%s %s"), Pos[i], *ANLFuseBox::GetFuseColorName(Want).ToString())));
		}
	}
}
