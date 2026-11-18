// NoLightAfterMidnight - HUD: soğuk
// Ekranın kenarlarından içeri doğru büyüyen soluk kırağı (yazı / gösterge yok: buzun kendisi uyarı).
// Isınırken buz erir (geri çekilir).
#include "NLHUDWidget.h"
#include "NLPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace NLFrost
{
	float Hash2(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u + uint32(Seed) * 2246822519u;
		H = (H ^ (H >> 13u)) * 1274126177u;
		return float(H ^ (H >> 16u)) / 4294967295.f;
	}

	// Yumuşak 2B değer gürültüsü
	float Noise2(float X, float Y, int32 Seed)
	{
		const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
		const float FX = X - IX, FY = Y - IY;
		const float SX = FX * FX * (3.f - 2.f * FX), SY = FY * FY * (3.f - 2.f * FY);
		const float A = FMath::Lerp(Hash2(IX, IY, Seed), Hash2(IX + 1, IY, Seed), SX);
		const float B = FMath::Lerp(Hash2(IX, IY + 1, Seed), Hash2(IX + 1, IY + 1, Seed), SX);
		return FMath::Lerp(A, B, SY);
	}

	float Fbm(float X, float Y, int32 Seed, int32 Octaves)
	{
		float Sum = 0.f, Amp = 0.5f, Norm = 0.f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += Amp * Noise2(X, Y, Seed + O * 17);
			Norm += Amp;
			X *= 2.03f; Y *= 2.03f; Amp *= 0.5f;
		}
		return Sum / Norm;
	}

	// Hücre gürültüsü: en yakın iki nokta mesafesi (F2-F1 küçükse buz kristalinin kenarı)
	float CellEdge(float X, float Y, int32 Seed)
	{
		const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
		float F1 = 9.f, F2 = 9.f;
		for (int32 J = -1; J <= 1; ++J)
		{
			for (int32 I = -1; I <= 1; ++I)
			{
				const int32 CX = IX + I, CY = IY + J;
				const float D = FMath::Sqrt(FMath::Square(CX + Hash2(CX, CY, Seed) - X) + FMath::Square(CY + Hash2(CX, CY, Seed + 7) - Y));
				if (D < F1) { F2 = F1; F1 = D; }
				else if (D < F2) { F2 = D; }
			}
		}
		return F2 - F1;
	}

	FColor FrostPixel(int32 PX, int32 PY, int32 N)
	{
		const float U = (PX + 0.5f) / N * 2.f - 1.f;
		const float V = (PY + 0.5f) / N * 2.f - 1.f;
		// Köşeleri yuvarlatılmış kare mesafesi: buz kenarlardan ve en çok köşelerden gelir
		const float D = FMath::Pow(FMath::Pow(FMath::Abs(U), 4.f) + FMath::Pow(FMath::Abs(V), 4.f), 0.25f) * 0.7f + FMath::Sqrt(U * U + V * V) * 0.3f;
		// Camdaki / lensteki kırağı: kenarlardan içeri doğru soluk beyaz pus, kenara doğru ışınsal büyüyen ince tüy kristaller
		const float X = PX / float(N), Y = PY / float(N);
		const float Base = Fbm(X * 5.f, Y * 5.f, 3, 5);
		const float Reach = D + (Base - 0.5f) * 0.45f;
		const float Haze = FMath::Pow(FMath::SmoothStep(0.62f, 1.12f, Reach), 1.4f);
		const float Rime = Fbm(X * 60.f, Y * 60.f, 21, 3);
		// İnce kristal yüzeyleri (çok soluk)
		const float Fine = (1.f - FMath::SmoothStep(0.f, 0.06f, CellEdge(X * 34.f, Y * 34.f, 9))) * FMath::SmoothStep(0.6f, 1.f, Reach);
		// Tüy kristaller: merkeze doğru uzanan ışınsal çizgiler (yön çember üstünde örneklenir: dikiş olmaz)
		const float Len = FMath::Max(FMath::Sqrt(U * U + V * V), 0.001f);
		const float Nf = Fbm(U / Len * 9.f + Base * 2.f, V / Len * 9.f + D * 3.f, 41, 3);
		const float Feather = FMath::Pow(1.f - FMath::Abs(2.f * Nf - 1.f), 9.f) * FMath::SmoothStep(0.55f, 0.95f, Reach);
		const float A = FMath::Clamp(Haze * (0.5f + 0.25f * Rime) + Fine * 0.06f + Feather * 0.26f, 0.f, 0.7f);
		const float Bright = FMath::Clamp(0.86f + 0.14f * Feather + 0.08f * Rime, 0.f, 1.f);
		const FLinearColor C(0.9f * Bright, 0.95f * Bright, 1.f * Bright, 1.f);
		const FColor Out = C.ToFColor(true);
		return FColor(Out.R, Out.G, Out.B, uint8(FMath::RoundToInt(A * 255.f)));
	}
}

void UNLHUDWidget::BuildColdOverlay(UCanvasPanel* Root)
{
	using namespace NLFrost;
	const int32 N = 512;
	UTexture2D* Tex = MakeProceduralTexture(N, N, [N](int32 X, int32 Y) { return FrostPixel(X, Y, N); }, false);
	if (Tex) { Tex->AddressX = TA_Clamp; Tex->AddressY = TA_Clamp; Tex->UpdateResource(); }
	FrostOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FrostOverlay"));
	FSlateBrush Brush;
	Brush.SetResourceObject(Tex);
	Brush.ImageSize = FVector2D(N, N);
	FrostOverlay->SetBrush(Brush);
	FrostOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	FrostOverlay->SetRenderOpacity(0.f);
	FrostOverlay->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Root->InsertChildAt(0, FrostOverlay);
	if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(FrostOverlay->Slot))
	{
		S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		S->SetOffsets(FMargin(0.f));
	}

}

void UNLHUDWidget::TickColdOverlay(float InDeltaTime, const ANLPlayerCharacter* Player)
{
	if (!FrostOverlay || !Player) return;
	const bool bAlive = Player->GetMode() != ENLPlayerMode::Dead;
	const float Target = Player->IsColdActive() && bAlive ? Player->GetColdAlpha() : 0.f;
	// Buz yavaş büyür, ısınınca daha hızlı erir
	ColdShown = FMath::FInterpTo(ColdShown, Target, InDeltaTime, Target > ColdShown ? 1.2f : 2.5f);
	// Soluk: en donmuş halde bile görüş açık kalır
	FrostOverlay->SetRenderOpacity(FMath::Clamp(FMath::Pow(ColdShown, 0.8f) * 0.62f, 0.f, 0.62f));
	// Büyüdükçe kenarlardan içeri sürünür (büyük ölçekte buz ekran dışında kalır)
	const float Scale = FMath::Lerp(1.5f, 1.02f, FMath::SmoothStep(0.f, 1.f, ColdShown));
	FrostOverlay->SetRenderScale(FVector2D(Scale, Scale));
}
