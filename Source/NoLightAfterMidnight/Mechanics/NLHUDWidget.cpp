#include "NLHUDWidget.h"
#include "NLGameMode.h"
#include "NLPlayerCharacter.h"
#include "NLSecurityDesk.h"
#include "NLSecurityCamera.h"
#include "NLGenerator.h"
#include "NLFlashlightComponent.h"
#include "NLPowerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Misc/DateTime.h"

// ------------------------------------------------------------------ Düzen (C++)

TSharedRef<SWidget> UNLHUDWidget::RebuildWidget()
{
	// Editörde tasarlanmış bir WBP yoksa arayüzü burada kur
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return Super::RebuildWidget();
}

static UCanvasPanelSlot* PlaceOnCanvas(UCanvasPanel* Canvas, UWidget* W, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FVector2D Size, bool bAutoSize)
{
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(W);
	Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
	Slot->SetAlignment(Alignment);
	Slot->SetPosition(Position);
	Slot->SetAutoSize(bAutoSize);
	if (!bAutoSize) Slot->SetSize(Size);
	return Slot;
}

static UCanvasPanelSlot* FillCanvas(UCanvasPanel* Canvas, UWidget* W, float Margin = 0.f)
{
	UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(W);
	Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	Slot->SetOffsets(FMargin(Margin));
	return Slot;
}

UTextBlock* UNLHUDWidget::MakeText(UCanvasPanel* Canvas, FName Name, const FString& Text, int32 Size, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FLinearColor Color)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	T->SetText(FText::FromString(Text));
	FSlateFontInfo Font = T->GetFont();
	Font.Size = Size;
	T->SetFont(Font);
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetShadowOffset(FVector2D(1.f, 1.f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	PlaceOnCanvas(Canvas, T, Anchor, Alignment, Position, FVector2D::ZeroVector, true);
	return T;
}

UProgressBar* UNLHUDWidget::MakeBar(UCanvasPanel* Canvas, FName Name, FVector2D Anchor, FVector2D Alignment, FVector2D Position, FVector2D Size, FLinearColor Fill)
{
	UProgressBar* B = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
	B->SetFillColorAndOpacity(Fill);
	B->SetPercent(1.f);
	PlaceOnCanvas(Canvas, B, Anchor, Alignment, Position, Size, false);
	return B;
}

void UNLHUDWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	const FLinearColor Dim(0.75f, 0.75f, 0.75f, 1.f);
	const FLinearColor Red(0.85f, 0.12f, 0.1f, 1.f);

	// ---- Kamera arayüzü (masada, elektrik varken) ----
	BuildCameraLayout(Root);

	// ---- Elektrik yok ("UI kapanmış hali") ----
	{
		UBorder* NoPower = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NoPowerPanel"));
		NoPower->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 1.f));
		NoPower->SetHorizontalAlignment(HAlign_Center);
		NoPower->SetVerticalAlignment(VAlign_Center);
		FillCanvas(Root, NoPower);
		NoPowerPanel = NoPower;

		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NoPowerText"));
		T->SetText(FText::FromString(TEXT("SİNYAL YOK\nELEKTRİK KESİK")));
		T->SetJustification(ETextJustify::Center);
		FSlateFontInfo F = T->GetFont(); F.Size = 32; T->SetFont(F);
		T->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f)));
		NoPower->SetContent(T);
	}

	// ---- Her zaman görünenler ----
	DayText   = MakeText(Root, TEXT("DayText"), TEXT("GECE 1 / 7"), 20, FVector2D(1, 0), FVector2D(1, 0), FVector2D(-70, 100), Dim);
	ClockText = MakeText(Root, TEXT("ClockText"), TEXT("12:00 AM"), 26, FVector2D(1, 0), FVector2D(1, 0), FVector2D(-70, 125));
	GeneratorStatusText = MakeText(Root, TEXT("GeneratorStatusText"), TEXT(""), 24, FVector2D(0.5f, 0), FVector2D(0.5f, 0), FVector2D(0, 60), Red);
	InteractPromptText  = MakeText(Root, TEXT("InteractPromptText"), TEXT(""), 20, FVector2D(0.5f, 0.6f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	FuelCanText = MakeText(Root, TEXT("FuelCanText"), TEXT("Benzin bidonu taşıyorsun"), 16, FVector2D(0, 1), FVector2D(0, 1), FVector2D(40, -60), Dim);
	FlashlightLabel = MakeText(Root, TEXT("FlashlightLabel"), TEXT("FENER [F]"), 14, FVector2D(0, 1), FVector2D(0, 1), FVector2D(40, -32), Dim);
	FlashlightBar = MakeBar(Root, TEXT("FlashlightBar"), FVector2D(0, 1), FVector2D(0, 1), FVector2D(130, -34), FVector2D(160, 12), FLinearColor(1.f, 0.85f, 0.4f));

	// ---- Tamir mini oyunu ----
	{
		UCanvasPanel* Repair = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RepairPanel"));
		PlaceOnCanvas(Root, Repair, FVector2D(0.5f, 0.75f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(RepairTrackWidth, 90), false);
		RepairPanel = Repair;

		MakeText(Repair, TEXT("RepairLabel"), TEXT("TAMİR EDİLİYOR...  yeşil bölgede [SPACE]"), 18, FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 0));

		UImage* Track = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RepairTrack"));
		Track->SetColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f));
		PlaceOnCanvas(Repair, Track, FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 30), FVector2D(RepairTrackWidth, 24), false);

		UImage* Zone = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RepairZone"));
		Zone->SetColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.3f, 0.9f));
		PlaceOnCanvas(Repair, Zone, FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 30), FVector2D(1, 24), false);
		RepairZone = Zone;

		UImage* Needle = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RepairNeedle"));
		Needle->SetColorAndOpacity(FLinearColor::White);
		PlaceOnCanvas(Repair, Needle, FVector2D(0, 0), FVector2D(0, 0), FVector2D(-2, 26), FVector2D(4, 32), false);
		RepairNeedle = Needle;

		RepairProgressBar = MakeBar(Repair, TEXT("RepairProgressBar"), FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 66), FVector2D(RepairTrackWidth, 12), FLinearColor(0.3f, 0.8f, 0.35f));
	}

	// ---- Game over ----
	{
		UBorder* GO = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameOverPanel"));
		GO->SetBrushColor(FLinearColor(0.25f, 0.f, 0.f, 0.85f));
		FillCanvas(Root, GO);
		GameOverPanel = GO;
	}

	CenterMessageText = MakeText(Root, TEXT("CenterMessageText"), TEXT(""), 64, FVector2D(0.5f, 0.45f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	CenterMessageText->SetJustification(ETextJustify::Center);
}

// ------------------------------------------------------------------ CCTV düzeni

static FSlateFontInfo CCTVFont(int32 Size)
{
	// Eski güvenlik kaydı yazısı için sabit genişlikli font
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), Size);
	Font.LetterSpacing = 80;
	return Font;
}

static FSlateBrush MakeTextureBrush(UTexture2D* Texture, FVector2D ImageSize, bool bTile)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = ImageSize;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = bTile ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
	return Brush;
}

UTexture2D* UNLHUDWidget::MakeProceduralTexture(int32 W, int32 H, TFunctionRef<FColor(int32, int32)> Pixel, bool bNearest)
{
	TArray64<uint8> Bytes;
	Bytes.SetNumUninitialized(int64(W) * H * 4);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const FColor C = Pixel(X, Y);
			const int64 I = (int64(Y) * W + X) * 4;
			Bytes[I + 0] = C.B;
			Bytes[I + 1] = C.G;
			Bytes[I + 2] = C.R;
			Bytes[I + 3] = C.A;
		}
	}

	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, NAME_None, Bytes);
	if (!Tex) return nullptr;
	Tex->Filter = bNearest ? TF_Nearest : TF_Bilinear;
	Tex->AddressX = TA_Wrap;
	Tex->AddressY = TA_Wrap;
	Tex->LODGroup = TEXTUREGROUP_UI;
	Tex->NeverStream = true;
	Tex->UpdateResource();
	GeneratedTextures.Add(Tex);
	return Tex;
}

UImage* UNLHUDWidget::MakeRect(UCanvasPanel* Canvas, FName Name, FVector2D Position, FVector2D Size, FLinearColor Color)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Img->SetColorAndOpacity(Color);
	PlaceOnCanvas(Canvas, Img, FVector2D(0, 0), FVector2D(0, 0), Position, Size, false);
	return Img;
}

void UNLHUDWidget::BuildCameraLayout(UCanvasPanel* Root)
{
	const FLinearColor White(0.92f, 0.92f, 0.92f, 1.f);

	UCanvasPanel* Cam = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CameraPanel"));
	FillCanvas(Root, Cam);
	CameraPanel = Cam;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CameraFrame"));
	Frame->SetBrushColor(FLinearColor::Black);
	FillCanvas(Cam, Frame);

	// Kamera görüntüsü tüm ekranı kaplar
	CameraFeedImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CameraFeedImage"));
	FillCanvas(Cam, CameraFeedImage);

	// --- Filtre katmanları: tarama çizgileri, kayan bant, karıncalanma, köşe kararması ---
	if (UTexture2D* ScanTex = MakeProceduralTexture(2, 4, [](int32, int32 Y)
		{
			const uint8 A = Y == 0 ? 70 : (Y == 1 ? 18 : 0);
			return FColor(0, 0, 0, A);
		}, true))
	{
		UImage* Scan = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FeedScanlines"));
		Scan->SetBrush(MakeTextureBrush(ScanTex, FVector2D(2, 4), true));
		FillCanvas(Cam, Scan);
	}

	if (UTexture2D* RollTex = MakeProceduralTexture(1, 64, [](int32, int32 Y)
		{
			const float S = FMath::Sin(PI * (Y + 0.5f) / 64.f);
			return FColor(255, 255, 255, uint8(S * S * 255.f));
		}, false))
	{
		FeedRollBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FeedRollBar"));
		FeedRollBar->SetBrush(MakeTextureBrush(RollTex, FVector2D(1, 64), false));
		FeedRollBar->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.045f));
		UCanvasPanelSlot* OverlaySlot = Cam->AddChildToCanvas(FeedRollBar);
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
		OverlaySlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 180.f));
	}

	FRandomStream Stream(1337);
	if (UTexture2D* NoiseTex = MakeProceduralTexture(256, 256, [&Stream](int32, int32)
		{
			const uint8 V = uint8(Stream.RandRange(0, 255));
			return FColor(V, V, V, 255);
		}, true))
	{
		// Ekrandan büyük çizilir, her karede rastgele kaydırılarak "canlı" karıncalanma elde edilir
		FeedNoiseImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FeedNoiseImage"));
		FeedNoiseImage->SetBrush(MakeTextureBrush(NoiseTex, FVector2D(512, 512), true));
		FeedNoiseImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, NoiseOpacity));
		FeedNoiseImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UCanvasPanelSlot* OverlaySlot = Cam->AddChildToCanvas(FeedNoiseImage);
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(-512.f, -512.f, 0.f, 0.f));
	}

	if (UTexture2D* VigTex = MakeProceduralTexture(128, 128, [](int32 X, int32 Y)
		{
			const float NX = (X + 0.5f) / 64.f - 1.f;
			const float NY = (Y + 0.5f) / 64.f - 1.f;
			const float D = FMath::Sqrt(NX * NX + NY * NY) / UE_SQRT_2;
			const float T = FMath::SmoothStep(0.38f, 1.f, D);
			return FColor(0, 0, 0, uint8(FMath::Clamp(FMath::Pow(T, 1.1f) * 245.f, 0.f, 255.f)));
		}, false))
	{
		VigTex->AddressX = TA_Clamp;
		VigTex->AddressY = TA_Clamp;
		VigTex->UpdateResource();
		UImage* Vig = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FeedVignette"));
		Vig->SetBrush(MakeTextureBrush(VigTex, FVector2D(128, 128), false));
		FillCanvas(Cam, Vig);
	}

	// --- Sol üst: "CAM 1 KİLER" ---
	CameraNameText = MakeText(Cam, TEXT("CameraNameText"), TEXT("CAM 1"), 30, FVector2D(0, 0), FVector2D(0, 0), FVector2D(48, 34), White);
	CameraNameText->SetFont(CCTVFont(30));

	// --- Sağ üst: kamera haritası kutusu ---
	{
		UBorder* MapBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CameraMapBox"));
		MapBox->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		MapBox->SetPadding(FMargin(16.f, 10.f));
		PlaceOnCanvas(Cam, MapBox, FVector2D(1, 0), FVector2D(1, 0), FVector2D(-40, 30), FVector2D::ZeroVector, true);

		CameraMapText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CameraMapText"));
		CameraMapText->SetFont(CCTVFont(16));
		CameraMapText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.f)));
		CameraMapText->SetText(FText::FromString(TEXT("GECE 1/7")));
		MapBox->SetContent(CameraMapText);
	}

	// --- Orta: odak nişangahı (ışık hazırsa parlak, cooldown'da sönük) ---
	{
		const float S = 72.f, L = 16.f, T = 3.f;
		UCanvasPanel* Reticle = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FlashReticle"));
		PlaceOnCanvas(Cam, Reticle, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0, 9), FVector2D(S, S + 18), false);

		const FVector2D Corners[4] = { {0, 0}, {S - L, 0}, {0, S - L}, {S - L, S - L} };
		for (int32 i = 0; i < 4; ++i)
		{
			const bool bRight = i % 2 == 1, bBottom = i >= 2;
			// yatay kol
			ReticleParts.Add(MakeRect(Reticle, *FString::Printf(TEXT("ReticleH%d"), i),
				FVector2D(Corners[i].X, bBottom ? S - T : 0.f), FVector2D(L, T), White));
			// dikey kol
			ReticleParts.Add(MakeRect(Reticle, *FString::Printf(TEXT("ReticleV%d"), i),
				FVector2D(bRight ? S - T : 0.f, Corners[i].Y), FVector2D(T, L), White));
		}
		UImage* Dot = MakeRect(Reticle, TEXT("ReticleDot"), FVector2D(S * 0.5f - 5.f, S * 0.5f - 5.f), FVector2D(10, 10), White);
		Dot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Dot->SetRenderTransformAngle(45.f);
		ReticleParts.Add(Dot);

		MakeRect(Reticle, TEXT("FlashReadyTrack"), FVector2D(0, S + 12), FVector2D(S, 3), FLinearColor(1.f, 1.f, 1.f, 0.18f));
		FlashReadyFill = MakeRect(Reticle, TEXT("FlashReadyFill"), FVector2D(0, S + 12), FVector2D(S, 3), White);
		FlashReadyFill->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	}

	// --- Sol alt: pil simgesi = jeneratör benzini ---
	{
		UCanvasPanel* Battery = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FuelBattery"));
		PlaceOnCanvas(Cam, Battery, FVector2D(0, 1), FVector2D(0, 1), FVector2D(48, -36), FVector2D(60, 26), false);

		MakeRect(Battery, TEXT("BatteryOutline"), FVector2D(0, 0), FVector2D(52, 26), White);
		MakeRect(Battery, TEXT("BatteryInner"), FVector2D(3, 3), FVector2D(46, 20), FLinearColor::Black);
		MakeRect(Battery, TEXT("BatteryNub"), FVector2D(52, 8), FVector2D(5, 10), White);
		for (int32 i = 0; i < 4; ++i)
		{
			BatterySegments.Add(MakeRect(Battery, *FString::Printf(TEXT("BatterySeg%d"), i), FVector2D(6 + i * 10.75f, 6), FVector2D(8.5f, 14), White));
		}

		FuelPercentText = MakeText(Cam, TEXT("FuelPercentText"), TEXT("%100"), 18, FVector2D(0, 1), FVector2D(0, 1), FVector2D(120, -36), White);
		FuelPercentText->SetFont(CCTVFont(18));
	}

	// --- Sağ alt: kayıt tarihi ve saati ---
	FeedTimestampText = MakeText(Cam, TEXT("FeedTimestampText"), TEXT("2025-03-12 00:00"), 30, FVector2D(1, 1), FVector2D(1, 1), FVector2D(-48, -30), White);
	FeedTimestampText->SetFont(CCTVFont(30));

	// --- Alt orta: kontroller ---
	UTextBlock* Help = MakeText(Cam, TEXT("CamHelp"), TEXT("[A/D] KAMERA  [1-8] SEÇ  [SPACE] IŞIK  [E] KALK"), 13,
		FVector2D(0.5f, 1), FVector2D(0.5f, 1), FVector2D(0, -34), FLinearColor(0.7f, 0.7f, 0.7f, 0.55f));
	Help->SetFont(CCTVFont(13));
}

bool UNLHUDWidget::GetClock(int32& OutHour, int32& OutMinute) const
{
	const ANLGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ANLGameMode>() : nullptr;
	if (!GM) return false;
	const float Hours = NightStartHour + (NightEndHour - NightStartHour) * GM->GetDayProgress();
	OutHour = FMath::FloorToInt(Hours);
	OutMinute = FMath::Clamp(FMath::FloorToInt((Hours - OutHour) * 60.f), 0, 59);
	return true;
}

void UNLHUDWidget::TickCameraOverlay(const FGeometry& MyGeometry, float InDeltaTime, const ANLSecurityDesk* Desk, const ANLGenerator* Generator)
{
	const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	const ANLSecurityCamera* Cam = Desk ? Desk->GetActiveCamera() : nullptr;
	const int32 ActiveIndex = Desk ? Desk->GetActiveIndex() : INDEX_NONE;
	const float Now = GetWorld()->GetRealTimeSeconds();

	// Kamera değişti / ekran yeni açıldı: kısa parazit
	if (ActiveIndex != LastCameraIndex)
	{
		LastCameraIndex = ActiveIndex;
		SwitchStaticTimer = SwitchStaticDuration;
	}
	SwitchStaticTimer = FMath::Max(0.f, SwitchStaticTimer - InDeltaTime);
	const float Static = SwitchStaticDuration > 0.f ? SwitchStaticTimer / SwitchStaticDuration : 0.f;

	// Karıncalanma: sabit kare hızında rastgele kaydır/çevir
	NoiseTimer -= InDeltaTime;
	if (NoiseTimer <= 0.f)
	{
		NoiseTimer = 1.f / FMath::Max(1.f, NoiseFPS);
		if (FeedNoiseImage)
		{
			FeedNoiseImage->SetRenderTranslation(FVector2D(FMath::FRandRange(0.f, 512.f), FMath::FRandRange(0.f, 512.f)));
			FeedNoiseImage->SetRenderScale(FVector2D(FMath::RandBool() ? 1.f : -1.f, FMath::RandBool() ? 1.f : -1.f));
		}
		if (CameraFeedImage)
		{
			const float Flicker = FMath::FRandRange(0.9f, 1.f) * (1.f - 0.5f * Static);
			CameraFeedImage->SetColorAndOpacity(FLinearColor(Flicker, Flicker, Flicker, 1.f));
		}
	}
	if (FeedNoiseImage)
	{
		FeedNoiseImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(NoiseOpacity + 0.75f * Static, 0.f, 1.f)));
	}

	// Ekrandan aşağı kayan parlak bant
	if (FeedRollBar)
	{
		const float H = MyGeometry.GetLocalSize().Y;
		const float BandH = 180.f;
		if (RollBarY < -BandH || RollBarY > H) RollBarY = -BandH;
		RollBarY += InDeltaTime * (H + BandH) / FMath::Max(0.5f, RollBarPeriod);
		FeedRollBar->SetRenderTranslation(FVector2D(0.f, RollBarY));
	}

	// Sol üst: CAM n YER
	if (CameraNameText && Cam)
	{
		const FText Label = Cam->GetLocationLabel().IsEmpty() ? Cam->GetCameraName() : Cam->GetLocationLabel();
		CameraNameText->SetText(FText::FromString(FString::Printf(TEXT("CAM %d %s"), ActiveIndex + 1, *Label.ToUpper().ToString())));
	}

	// Sağ üst: gece + kamera ızgarası, aktif olan [ ] içinde
	if (CameraMapText && Desk)
	{
		FString Map = GM ? FString::Printf(TEXT("GECE %d/%d\n"), FMath::Max(1, GM->GetCurrentDay()), GM->GetTotalDays()) : FString();
		const int32 Num = Desk->GetCameras().Num();
		for (int32 i = 0; i < Num; ++i)
		{
			Map += (i == ActiveIndex) ? FString::Printf(TEXT("[%d]"), i + 1) : FString::Printf(TEXT(" %d "), i + 1);
			if (i % 4 == 3 && i != Num - 1) Map += TEXT("\n");
		}
		CameraMapText->SetText(FText::FromString(Map));
	}

	// Orta: nişangah ışık hazır mı
	if (Cam)
	{
		const float Ready = 1.f - Cam->GetCooldownPercent();
		const bool bReady = Cam->GetCooldownRemaining() <= 0.f;
		const FLinearColor C = bReady ? FLinearColor(0.95f, 0.95f, 0.95f, 1.f) : FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
		for (UImage* Part : ReticleParts) if (Part) Part->SetColorAndOpacity(C);
		if (FlashReadyFill) FlashReadyFill->SetRenderScale(FVector2D(FMath::Clamp(Ready, 0.f, 1.f), 1.f));
		if (FlashCooldownBar) FlashCooldownBar->SetPercent(Ready);
	}

	// Sol alt: pil (benzin). Son dilim kaldığında yanıp söner.
	if (Generator)
	{
		const float Fuel = FMath::Clamp(Generator->GetFuelPercent(), 0.f, 1.f);
		const int32 Lit = FMath::CeilToInt(Fuel * BatterySegments.Num() - KINDA_SMALL_NUMBER);
		const bool bBlinkOff = Fuel < 0.25f && FMath::Fmod(Now, 1.f) > 0.5f;
		for (int32 i = 0; i < BatterySegments.Num(); ++i)
		{
			if (!BatterySegments[i]) continue;
			const bool bOn = i < Lit && !bBlinkOff;
			BatterySegments[i]->SetColorAndOpacity(bOn ? FLinearColor(0.92f, 0.92f, 0.92f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.08f));
		}
		if (FuelPercentText) FuelPercentText->SetText(FText::FromString(FString::Printf(TEXT("%%%d"), FMath::RoundToInt(Fuel * 100.f))));
		if (FuelBar) FuelBar->SetPercent(Fuel);
	}

	// Sağ alt: YYYY-AA-GG SS:DD
	int32 Hour = 0, Minute = 0;
	if (FeedTimestampText && GM && GetClock(Hour, Minute))
	{
		FDateTime Date(2025, 1, 1);
		if (FDateTime::Validate(FeedStartYear, FeedStartMonth, FeedStartDay, 0, 0, 0, 0))
		{
			Date = FDateTime(FeedStartYear, FeedStartMonth, FeedStartDay);
		}
		Date += FTimespan::FromDays(FMath::Max(0, GM->GetCurrentDay() - 1));
		FeedTimestampText->SetText(FText::FromString(FString::Printf(TEXT("%04d-%02d-%02d %02d:%02d"),
			Date.GetYear(), Date.GetMonth(), Date.GetDay(), Hour % 24, Minute)));
	}
}

// ------------------------------------------------------------------ Yaşam döngüsü

void UNLHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		GM->OnDayStarted.AddUniqueDynamic(this, &UNLHUDWidget::HandleDayStarted);
		GM->OnDayEnded.AddUniqueDynamic(this, &UNLHUDWidget::HandleDayEnded);
		GM->OnShowGameOverScreen.AddUniqueDynamic(this, &UNLHUDWidget::HandleGameOverScreen);
		GM->OnGameWon.AddUniqueDynamic(this, &UNLHUDWidget::HandleGameWon);
	}

	// Tamir bölgesi soldan ölçeklensin
	if (RepairZone) RepairZone->SetRenderTransformPivot(FVector2D(0.f, 0.5f));

	SetShown(CameraPanel, false);
	SetShown(NoPowerPanel, false);
	SetShown(GameOverPanel, false);
	SetShown(RepairPanel, false);
	SetShown(CenterMessageText, false);
}

void UNLHUDWidget::SetShown(UWidget* W, bool bShown)
{
	if (W) W->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UNLHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	const ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(GetOwningPlayerPawn());
	const UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
	const ANLGenerator* Generator = Power ? Power->GetGenerator() : nullptr;
	const bool bHasPower = Power && Power->HasPower();

	// --- Gün & saat ---
	if (GM)
	{
		if (DayText) DayText->SetText(FText::FromString(FString::Printf(TEXT("GECE %d / %d"), FMath::Max(1, GM->GetCurrentDay()), GM->GetTotalDays())));
		int32 H = 0, M = 0;
		if (ClockText && GetClock(H, M))
		{
			ClockText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d AM"), H == 0 ? 12 : H, M)));
		}
	}

	if (!Player) return;

	const ENLPlayerMode Mode = Player->GetMode();
	const bool bSeated = Mode == ENLPlayerMode::Seated;

	// Masadayken alt yazıyı kamera arayüzü gösteriyor
	if (InteractPromptText) InteractPromptText->SetText(bSeated ? FText::GetEmpty() : Player->GetCurrentInteractPrompt());
	if (FlashlightBar && Player->GetFlashlight()) FlashlightBar->SetPercent(Player->GetFlashlight()->GetBatteryPercent());
	SetShown(FuelCanText, Player->HasFuelCan() && !bSeated);

	// Masadayken yürüme arayüzü gizlenir; saat/gece kamera ekranında gösterilir
	SetShown(DayText, !bSeated);
	SetShown(ClockText, !bSeated);
	SetShown(FlashlightBar, !bSeated);
	SetShown(FlashlightLabel, !bSeated);

	// --- Kamera arayüzü ---
	SetShown(CameraPanel, bSeated && bHasPower);
	SetShown(NoPowerPanel, bSeated && !bHasPower);

	if (bSeated && bHasPower)
	{
		const ANLSecurityDesk* Desk = Player->GetCurrentDesk();

		if (CameraFeedImage && Desk)
		{
			UTextureRenderTarget2D* RT = Desk->GetFeedRenderTarget();
			if (RT && BoundFeedTexture.Get() != RT)
			{
				CameraFeedImage->SetBrushResourceObject(RT);
				BoundFeedTexture = RT;
			}
		}
		TickCameraOverlay(MyGeometry, InDeltaTime, Desk, Generator);
	}
	else
	{
		// Ekran tekrar açıldığında parazitle başlasın
		LastCameraIndex = INDEX_NONE;
	}

	if (GeneratorStatusText)
	{
		FString Status;
		if (Generator)
		{
			if (Generator->IsBroken()) Status = TEXT("JENERATÖR ARIZALI!");
			else if (Generator->GetFuelPercent() <= 0.f) Status = TEXT("BENZİN BİTTİ!");
			else if (Generator->GetFuelPercent() < 0.15f) Status = TEXT("BENZİN AZALIYOR");
		}
		if (Status.IsEmpty() && !bHasPower && Generator) Status = TEXT("ELEKTRİK YOK");
		GeneratorStatusText->SetText(FText::FromString(Status));
	}

	// --- Tamir barı ---
	const ANLGenerator* RepairGen = Player->GetCurrentGenerator();
	const bool bRepairing = Mode == ENLPlayerMode::Repairing && RepairGen;
	SetShown(RepairPanel, bRepairing);
	if (bRepairing)
	{
		if (RepairProgressBar) RepairProgressBar->SetPercent(RepairGen->GetRepairProgress());
		if (RepairNeedle) RepairNeedle->SetRenderTranslation(FVector2D(RepairGen->GetNeedlePosition() * RepairTrackWidth, 0.f));
		if (RepairZone)
		{
			RepairZone->SetRenderTranslation(FVector2D(RepairGen->GetZoneStart() * RepairTrackWidth, 0.f));
			RepairZone->SetRenderScale(FVector2D(FMath::Max(1.f, RepairGen->GetZoneWidth() * RepairTrackWidth), 1.f));
		}
	}

	// --- Orta mesaj soluklaşması ---
	if (CenterMessageText && !bPersistentMessage && CenterMessageTimer > 0.f)
	{
		CenterMessageTimer -= InDeltaTime;
		CenterMessageText->SetRenderOpacity(FMath::Clamp(CenterMessageTimer, 0.f, 1.f));
		if (CenterMessageTimer <= 0.f) SetShown(CenterMessageText, false);
	}
}

void UNLHUDWidget::ShowCenterMessage(const FString& Message, bool bPersistent)
{
	if (!CenterMessageText) return;
	CenterMessageText->SetText(FText::FromString(Message));
	CenterMessageText->SetRenderOpacity(1.f);
	SetShown(CenterMessageText, true);
	CenterMessageTimer = CenterMessageDuration;
	bPersistentMessage = bPersistent;
}

void UNLHUDWidget::HandleDayStarted(int32 Day)
{
	ShowCenterMessage(FString::Printf(TEXT("DAY %d"), Day), false);
}

void UNLHUDWidget::HandleDayEnded(int32 Day)
{
	ShowCenterMessage(TEXT("06:00 AM"), false);
}

void UNLHUDWidget::HandleGameOverScreen(ENLGameOverReason Reason)
{
	SetShown(GameOverPanel, true);
	ShowCenterMessage(TEXT("GAME OVER"), true);
}

void UNLHUDWidget::HandleGameWon()
{
	ShowCenterMessage(TEXT("FENER SÖNMEDİ.\n7 GECE TAMAMLANDI"), true);
}
