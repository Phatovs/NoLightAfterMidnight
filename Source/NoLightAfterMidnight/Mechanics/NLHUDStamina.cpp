// NoLightAfterMidnight - HUD: stamina
// Alt ortada ince, yıpranmış (çizik, düzensiz kenarlı) bir bar: iki yandan ortaya doğru kapanır.
// Doluyken soluk beyaz, azaldıkça griye döner ve arızalı ışık gibi titrer.
// Azaldıkça ekranın kenarları kararır (tünel görüşü) ve nabız gibi atar.
#include "NLHUDWidget.h"
#include "NLPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace NLStaminaBar
{
	constexpr int32 BarW = 512, BarH = 32;

	float Saturate(float X) { return FMath::Clamp(X, 0.f, 1.f); }

	float Hash(int32 N)
	{
		uint32 X = uint32(N) * 747796405u + 2891336453u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		return float((X >> 22u) ^ X) / 4294967295.f;
	}

	// Yumuşak 1B gürültü
	float Noise1(float X, int32 Seed)
	{
		const int32 I = FMath::FloorToInt(X);
		const float F = X - I;
		const float T = F * F * (3.f - 2.f * F);
		return FMath::Lerp(Hash(I * 13 + Seed), Hash((I + 1) * 13 + Seed), T);
	}

	FColor BarPixel(int32 X, int32 Y)
	{
		const float Cy = BarH * 0.5f;
		// Düzensiz kalınlık (elle çizilmiş / yıpranmış)
		const float Half = 7.f + 2.2f * Noise1(X / 23.f, 1) + 0.8f * Noise1(X / 5.f, 2);
		const float Off = (Noise1(X / 41.f, 3) - 0.5f) * 2.4f;   // hafif dalgalı çizgi
		const float Dist = FMath::Abs(Y + 0.5f - Cy - Off);
		float A = Saturate(Half - Dist + 0.5f);
		if (A <= 0.f) return FColor(255, 255, 255, 0);

		// Doku: gren, çizikler, kopuk yerler
		A *= 0.8f + 0.2f * Hash(X * 7919 + Y * 104729);
		if (Hash(X / 4 + 77) > 0.95f) A *= 0.55f;
		if (Noise1(X / 11.f, 4) > 0.85f) A *= 0.75f;
		// Uçlar yumuşakça incelir
		const float Edge = FMath::Min(X + 0.5f, BarW - X - 0.5f);
		A *= Saturate(Edge / 26.f);
		return FColor(255, 255, 255, uint8(FMath::RoundToInt(A * 255.f)));
	}

	FColor VignettePixel(int32 X, int32 Y, int32 N)
	{
		const float U = (X + 0.5f) / N * 2.f - 1.f;
		const float V = (Y + 0.5f) / N * 2.f - 1.f;
		const float R = FMath::Sqrt(U * U + V * V);
		float A = FMath::SmoothStep(0.45f, 1.25f, R);
		A *= 0.85f + 0.15f * Hash(X * 31 + Y * 977);   // biraz kirli
		const FColor C = FLinearColor(0.008f, 0.008f, 0.009f).ToFColor(true);
		return FColor(C.R, C.G, C.B, uint8(FMath::RoundToInt(Saturate(A) * 255.f)));
	}
}

void UNLHUDWidget::BuildStaminaGauge(UCanvasPanel* Root)
{
	using namespace NLStaminaBar;

	// Kararan ekran kenarları: diğer arayüzün arkasında kalsın diye en alta
	{
		const int32 N = 256;
		UTexture2D* VigTex = MakeProceduralTexture(N, N, [N](int32 X, int32 Y) { return VignettePixel(X, Y, N); }, false);
		if (VigTex) { VigTex->AddressX = TA_Clamp; VigTex->AddressY = TA_Clamp; VigTex->UpdateResource(); }
		StaminaVignette = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StaminaVignette"));
		FSlateBrush Brush;
		Brush.SetResourceObject(VigTex);
		Brush.ImageSize = FVector2D(N, N);
		StaminaVignette->SetBrush(Brush);
		StaminaVignette->SetVisibility(ESlateVisibility::HitTestInvisible);
		StaminaVignette->SetRenderOpacity(0.f);
		Root->InsertChildAt(0, StaminaVignette);
		if (UCanvasPanelSlot* VS = Cast<UCanvasPanelSlot>(StaminaVignette->Slot))
		{
			VS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			VS->SetOffsets(FMargin(0.f));
		}
	}

	StaminaGauge = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StaminaBar"));
	UCanvasPanelSlot* S = Root->AddChildToCanvas(StaminaGauge);
	S->SetAnchors(FAnchors(0.5f, 1.f));
	S->SetAlignment(FVector2D(0.5f, 1.f));
	S->SetPosition(FVector2D(0.f, -46.f));
	S->SetSize(FVector2D(StaminaBarWidth, 18.f));
	StaminaGauge->SetVisibility(ESlateVisibility::HitTestInvisible);
	StaminaGauge->SetRenderOpacity(0.f);

	UTexture2D* BarTex = MakeProceduralTexture(BarW, BarH, [](int32 X, int32 Y) { return BarPixel(X, Y); }, false);
	if (BarTex) { BarTex->AddressX = TA_Clamp; BarTex->AddressY = TA_Clamp; BarTex->UpdateResource(); }

	auto AddBar = [this, BarTex](FName Name) -> UImage*
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		FSlateBrush Brush;
		Brush.SetResourceObject(BarTex);
		Brush.ImageSize = FVector2D(StaminaBarWidth, 18.f);
		Img->SetBrush(Brush);
		UCanvasPanelSlot* IS = StaminaGauge->AddChildToCanvas(Img);
		IS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		IS->SetOffsets(FMargin(0.f));
		Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));   // iki yandan ortaya doğru kapanır
		return Img;
	};
	// Arkada koyu iz (boşalan kısım), önde dolu kısım
	StaminaTrack = AddBar(TEXT("StaminaTrack"));
	StaminaTrack->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	StaminaFill = AddBar(TEXT("StaminaFill"));
}

void UNLHUDWidget::TickStaminaGauge(float InDeltaTime, const ANLPlayerCharacter* Player)
{
	using namespace NLStaminaBar;
	if (!StaminaGauge || !StaminaFill || !Player) return;

	const float Value = Player->GetStamina();
	const bool bExhausted = Player->IsExhausted();
	const ENLPlayerMode Mode = Player->GetMode();
	const bool bActiveMode = Mode == ENLPlayerMode::Walking || Mode == ENLPlayerMode::HoldingDoor;

	// Dolu ve koşmuyorken bir süre sonra kaybolur
	if (Player->IsSprinting() || Value < 0.995f) StaminaHideTimer = 1.5f;
	else StaminaHideTimer -= InDeltaTime;
	const bool bShow = StaminaHideTimer > 0.f && bActiveMode;
	StaminaGaugeAlpha = FMath::FInterpTo(StaminaGaugeAlpha, bShow ? 1.f : 0.f, InDeltaTime, bShow ? 6.f : 1.5f);

	// Bar yumuşakça değere gider; iki yandan kapanır
	StaminaShown = FMath::FInterpTo(StaminaShown, Value, InDeltaTime, 10.f);
	StaminaFill->SetRenderScale(FVector2D(FMath::Max(StaminaShown, 0.002f), 1.f));

	// Azaldıkça soluk beyazdan grimsi, sönük bir renge (kırmızı değil: can değil enerji)
	StaminaLow = FMath::FInterpTo(StaminaLow, Saturate((0.55f - Value) / 0.55f), InDeltaTime, 4.f);
	FLinearColor Col = FMath::Lerp(FLinearColor(0.80f, 0.76f, 0.68f, 0.75f), FLinearColor(0.36f, 0.36f, 0.37f, 0.8f), StaminaLow);
	// Tamamen bittiyse: tekrar dolana kadar soluk, sönük gri (yavaş doluyor)
	StaminaDepletedFx = FMath::FInterpTo(StaminaDepletedFx, Player->IsStaminaDepleted() ? 1.f : 0.f, InDeltaTime, 3.f);
	Col = FMath::Lerp(Col, FLinearColor(0.42f, 0.42f, 0.43f, 0.42f), StaminaDepletedFx);

	// Arızalı ışık gibi titreme (azaldıkça sıklaşır), tükenince yavaşça söner-yanar
	StaminaPulsePhase = FMath::Fmod(StaminaPulsePhase + InDeltaTime * FMath::Lerp(1.1f, 2.6f, StaminaLow), 1.f);
	if (FMath::FRand() < 0.18f * StaminaLow) StaminaFlicker = FMath::FRandRange(0.25f, 0.7f);
	StaminaFlicker = FMath::FInterpTo(StaminaFlicker, 1.f, InDeltaTime, 18.f);
	float Bright = StaminaFlicker;
	if (bExhausted) Bright *= 0.45f + 0.35f * (0.5f + 0.5f * FMath::Sin(StaminaPulsePhase * 2.f * PI));
	Col.A *= Bright;
	StaminaFill->SetColorAndOpacity(Col);
	StaminaGauge->SetRenderOpacity(StaminaGaugeAlpha);
	StaminaGauge->SetRenderTranslation(bExhausted ? FVector2D(FMath::FRandRange(-1.2f, 1.2f), FMath::FRandRange(-0.6f, 0.6f)) : FVector2D::ZeroVector);

	// Ekran kenarları kararır ve nabız gibi atar (çift vuruş)
	if (StaminaVignette)
	{
		auto Beat = [](float T, float At) { const float X = (T - At) / 0.07f; return FMath::Exp(-X * X); };
		const float Pulse = Beat(StaminaPulsePhase, 0.1f) + 0.6f * Beat(StaminaPulsePhase, 0.3f);
		const float Dark = bActiveMode ? FMath::Pow(StaminaLow, 1.3f) * (0.72f + 0.2f * Pulse) + (bExhausted ? 0.12f : 0.f) : 0.f;
		StaminaVignette->SetRenderOpacity(FMath::Clamp(Dark, 0.f, 0.95f));
	}
}
