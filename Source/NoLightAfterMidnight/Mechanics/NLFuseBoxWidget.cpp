#include "NLFuseBoxWidget.h"
#include "NLFuseBox.h"
#include "NLPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UNLFuseBoxWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Build();
	return Super::RebuildWidget();
}

void UNLFuseBoxWidget::Build()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	// Tıklamaları yutma: fare oyuna (viewport) gitsin, her karede oradan okunur
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Altta küçük ipucu ve durum satırı
	auto MakeLine = [this, Root](const FString& Text, int32 Size, float FromBottom, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Text));
		T->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(1.f, 1.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		T->SetJustification(ETextJustify::Center);
		UCanvasPanelSlot* S = Root->AddChildToCanvas(T);
		S->SetAnchors(FAnchors(0.5f, 1.f));
		S->SetAlignment(FVector2D(0.5f, 1.f));
		S->SetAutoSize(true);
		S->SetPosition(FVector2D(0.f, -FromBottom));
		return T;
	};
	StatusText = MakeLine(TEXT(""), 18, 62.f, FLinearColor::White);
	MakeLine(TEXT("Hold and drag a fuse onto another slot to swap  -  Hold the lever and drag it up / down  -  [E] Back"), 14, 34.f, FLinearColor(0.8f, 0.8f, 0.8f, 0.85f));
}

void UNLFuseBoxWidget::Init(ANLFuseBox* InBox, ANLPlayerCharacter* InPlayer)
{
	Box = InBox;
	Player = InPlayer;
}

bool UNLFuseBoxWidget::TryPickFuse(int32 SlotIdx)
{
	ANLFuseBox* B = Box.Get();
	if (!B || SlotIdx == INDEX_NONE) return false;

	// Şalter açıkken sigortaya dokunmak: elektrik çarpar
	if (B->IsOn())
	{
		if (ANLPlayerCharacter* P = Player.Get()) P->ElectricShock();
		return false;
	}
	DragSlot = SlotIdx;
	B->BeginDrag(SlotIdx);
	return true;
}

void UNLFuseBoxWidget::DropFuse(int32 TargetSlot)
{
	ANLFuseBox* B = Box.Get();
	if (B && DragSlot != INDEX_NONE && TargetSlot != INDEX_NONE && TargetSlot != DragSlot)
	{
		B->SwapFuses(DragSlot, TargetSlot);
	}
	if (B) B->EndDrag();
	DragSlot = INDEX_NONE;
}

void UNLFuseBoxWidget::ToggleSwitch()
{
	if (ANLFuseBox* B = Box.Get()) B->SetOn(!B->IsOn());
}

void UNLFuseBoxWidget::UpdateMouse()
{
	ANLFuseBox* B = Box.Get();
	APlayerController* PC = GetOwningPlayer();
	if (!B || !PC) return;

	FVector O, D;
	const bool bRay = PC->DeprojectMousePositionToWorld(O, D);
	float MouseX = 0.f, MouseY = 0.f;
	PC->GetMousePosition(MouseX, MouseY);
	const bool bDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);

	if (bDown && !bMouseWasDown)
	{
		// Basıldı: şalter kolu mu, sigorta mı?
		if (bRay && B->RayHitsLever(O, D))
		{
			bLeverDrag = true;
			LeverStartMouseY = MouseY;
			LeverStartAngle = B->GetLeverAngle();
		}
		else if (bRay)
		{
			const int32 SlotIdx = B->RayPickFuse(O, D);
			if (SlotIdx != INDEX_NONE && TryPickFuse(SlotIdx))
			{
				B->UpdateDrag(O, D);
			}
		}
	}
	else if (bDown)
	{
		// Basılı tutuluyor: sürükle
		if (DragSlot != INDEX_NONE && bRay)
		{
			B->UpdateDrag(O, D);
		}
		if (bLeverDrag)
		{
			// Fareyi aşağı çekmek kolu indirir. Ekran yüksekliğinin ~%25'i tam hareket.
			FVector2D ViewSize(1920.f, 1080.f);
			if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(ViewSize);
			const float Range = FMath::Abs(B->GetLeverOnAngle() - B->GetLeverOffAngle());
			const float DegPerPixel = Range / FMath::Max(ViewSize.Y * 0.25f, 50.f);
			const float UpSign = B->GetLeverOnAngle() > B->GetLeverOffAngle() ? 1.f : -1.f;
			B->SetLeverDragAngle(LeverStartAngle - (MouseY - LeverStartMouseY) * DegPerPixel * UpSign);
		}
	}
	else if (!bDown && bMouseWasDown)
	{
		// Bırakıldı: her durumda sürükleme biter (takılı kalmaz)
		if (DragSlot != INDEX_NONE)
		{
			DropFuse(bRay ? B->RayPickSlot(O, D) : INDEX_NONE);
		}
		if (bLeverDrag)
		{
			bLeverDrag = false;
			B->EndLeverDrag();
		}
	}
	else if (bRay)
	{
		// Boşta: farenin üstündeki sigorta hafifçe büyür
		B->SetHoveredSlot(B->RayPickFuse(O, D));
	}

	bMouseWasDown = bDown;
}

void UNLFuseBoxWidget::UpdateStatus()
{
	ANLFuseBox* B = Box.Get();
	if (!B || !StatusText) return;

	FString S;
	FLinearColor C(0.9f, 0.9f, 0.9f);
	if (B->IsPowerOk()) { S = TEXT("POWER ON"); C = FLinearColor(0.35f, 1.f, 0.4f); }
	else if (B->IsOn()) { S = TEXT("Switch is ON - don't touch the fuses!"); C = FLinearColor(1.f, 0.4f, 0.3f); }
	else if (!B->AreFusesCorrect()) { S = TEXT("Switch OFF - safe to move the fuses"); C = FLinearColor(1.f, 0.82f, 0.35f); }
	else { S = TEXT("Fuses in place - pull the lever up"); C = FLinearColor(0.6f, 0.9f, 1.f); }
	StatusText->SetText(FText::FromString(S));
	StatusText->SetColorAndOpacity(FSlateColor(C));
}

void UNLFuseBoxWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateMouse();
	UpdateStatus();
}
