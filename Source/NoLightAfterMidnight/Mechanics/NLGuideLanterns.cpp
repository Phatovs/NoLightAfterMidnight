#include "NLGuideLanterns.h"
#include "NLEnvKit.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

using namespace NLEnv;

ANLGuideLanterns::ANLGuideLanterns()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Posts = MakeISM(this, Root, TEXT("Posts"), true);
	Signs = MakeISM(this, Root, TEXT("Signs"), true);
	for (UInstancedStaticMeshComponent* C : { Posts.Get(), Signs.Get() }) C->SetMobility(EComponentMobility::Movable);
	NL_KIT_MESH(Posts, "SM_NL_LanternPost");
	NL_KIT_MESH(Signs, "SM_NL_Sign");
	{
		static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> LanternFinder(TEXT("/Game/Environment/Kit/SM_NL_Lantern.SM_NL_Lantern"));
		LanternMesh = LanternFinder.Get();
	}

	SignLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignLabel"));
	SignLabel->SetupAttachment(Root);
	SignLabel->SetHorizontalAlignment(EHTA_Center);
	SignLabel->SetVerticalAlignment(EVRTA_TextCenter);
	SignLabel->SetWorldSize(13.f);
	SignLabel->SetTextRenderColor(FColor(205, 190, 160));
	SignLabel->SetVisibility(false);
}

bool ANLGuideLanterns::LoadMask()
{
	const FString File = FPaths::ProjectContentDir() / TEXT("Data/NLPathMask.png");
	TArray<uint8> Png;
	if (!FFileHelper::LoadFileToArray(Png, *File)) return false;
	IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> IW = IWM.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Raw;
	if (!IW.IsValid() || !IW->SetCompressed(Png.GetData(), Png.Num()) || !IW->GetRaw(ERGBFormat::RGBA, 8, Raw)) return false;
	MaskW = IW->GetWidth();
	MaskH = IW->GetHeight();
	Mask.SetNumUninitialized(MaskW * MaskH);
	for (int32 i = 0; i < MaskW * MaskH; ++i) Mask[i] = Raw[i * 4];
	return MaskW > 0 && MaskH > 0;
}

float ANLGuideLanterns::PathAt(const FVector& P) const
{
	if (Mask.Num() == 0) return 0.f;
	const int32 U = FMath::Clamp(FMath::FloorToInt((P.X - MaskMin) / MaskSize * MaskW), 0, MaskW - 1);
	const int32 V = FMath::Clamp(FMath::FloorToInt((P.Y - MaskMin) / MaskSize * MaskH), 0, MaskH - 1);
	return Mask[V * MaskW + U] / 255.f;
}

bool ANLGuideLanterns::FindRoadEdges(const FVector& P, const FVector& N, float& OutNeg, float& OutPos) const
{
	if (Mask.Num() == 0) return false;
	const float Step = 15.f, Range = 700.f, On = 0.3f;
	// Nav noktasına en yakın yol hücresini bul (nav yolu toprak yolun biraz yanından geçebilir)
	float Center = TNumericLimits<float>::Max();
	for (float t = 0.f; t <= Range; t += Step)
	{
		if (PathAt(P + N * t) > On) { Center = t; break; }
		if (PathAt(P - N * t) > On) { Center = -t; break; }
	}
	if (Center == TNumericLimits<float>::Max()) return false;
	float Pos = Center, Neg = Center;
	while (Pos - Center < 900.f && PathAt(P + N * (Pos + Step)) > On) Pos += Step;
	while (Center - Neg < 900.f && PathAt(P + N * (Neg - Step)) > On) Neg -= Step;
	OutNeg = Neg;
	OutPos = Pos;
	return true;
}

void ANLGuideLanterns::ScheduleFlicker(FNLGuideLamp& L, float Now, bool bFirst)
{
	// İlk titremeler fenerler arasında dağılsın, hepsi aynı anda yanıp sönmesin
	const float Wait = FMath::FRandRange(FlickerInterval.X, FlickerInterval.Y) * (bFirst ? FMath::FRandRange(0.2f, 1.f) : 1.f);
	L.NextFlicker = Now + Wait;
	L.bBlackout = FMath::FRand() < BlackoutChance;
	L.FlickerEnd = L.NextFlicker + (L.bBlackout ? FMath::FRandRange(2.f, 4.f) : FMath::FRandRange(0.4f, 1.3f));
}

void ANLGuideLanterns::Build(const FVector& Start, const FVector& Goal)
{
	UWorld* W = GetWorld();
	const bool bMask = LoadMask();
	TArray<FVector> Pts;
	// Başlangıç (rıhtım) ve hedef (masa) NavMesh üstünde olmayabilir: en yakın yürünür noktaya indir
	FVector S0 = Start, G0 = Goal;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W))
	{
		FNavLocation Out;
		if (Nav->ProjectPointToNavigation(Start, Out, FVector(600.f, 600.f, 800.f))) S0 = Out.Location;
		if (Nav->ProjectPointToNavigation(Goal, Out, FVector(600.f, 600.f, 800.f))) G0 = Out.Location;
	}
	if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(W, S0, G0))
	{
		if (Path->IsValid() && Path->PathPoints.Num() >= 2) Pts = Path->PathPoints;
	}
	const bool bNav = Pts.Num() >= 2;
	if (!bNav) Pts = { Start, Goal };

	float Total = 0.f;
	for (int32 i = 1; i < Pts.Num(); ++i) Total += FVector::Dist2D(Pts[i - 1], Pts[i]);

	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLGuide), false, this);
	auto Ground = [&](FVector P)
	{
		FHitResult Hit;
		if (W->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 1500.f), P - FVector(0, 0, 2000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z;
		return P;
	};
	// Direğin yerinde ağaç/kaya/bina var mı (zeminin biraz üstünde)
	auto Blocked = [&](const FVector& Base)
	{
		return W->OverlapAnyTestByChannel(Base + FVector(0, 0, 140.f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(28.f, 100.f), Q);
	};

	const float Now = W->GetTimeSeconds();
	int32 Count = 0, OnRoad = 0;
	bool bSign = false;
	float Walked = 0.f, Next = SkipStart;
	for (int32 i = 1; i < Pts.Num(); ++i)
	{
		const FVector A = Pts[i - 1], B = Pts[i];
		const float L = FVector::Dist2D(A, B);
		const FVector D = FVector(B.X - A.X, B.Y - A.Y, 0.f).GetSafeNormal();
		const FVector N(-D.Y, D.X, 0.f);
		while (Next <= Walked + L && Next <= Total - SkipEnd)
		{
			const FVector P = A + (B - A) * ((Next - Walked) / FMath::Max(L, 1.f));
			// Kenarlara sırayla: sağ, sol
			const float Side = (Count % 2 == 0) ? 1.f : -1.f;
			// Yolun kenarını bul; direk kenarın EdgeMargin dışında
			float Neg = 0.f, Pos = 0.f, Off = SideOffset * Side, RoadMid = 0.f;
			if (bMask && FindRoadEdges(P, N, Neg, Pos))
			{
				Off = (Side > 0.f ? Pos : Neg) + EdgeMargin * Side;
				RoadMid = (Neg + Pos) * 0.5f;
				++OnRoad;
			}
			FVector Base = Ground(P + N * Off);
			// Hâlâ yolun üstündeyse ya da ağaca/binaya denk geliyorsa biraz daha dışarı
			for (int32 k = 0; k < 8 && (PathAt(Base) > 0.2f || Blocked(Base)); ++k)
			{
				Off += 40.f * Side;
				Base = Ground(P + N * Off);
			}
			// Kol yola doğru uzanır
			const FVector Arm = (RoadMid - Off) * Side < 0.f ? -N * Side : N * Side;
			Posts->AddInstance(FTransform(Orient(Arm, FVector::UpVector), Base, FVector(1.f, 1.f, PostHeightScale)), true);
			const FVector LanternAt = Base + Arm * 78.f + FVector(0, 0, 292.f * PostHeightScale);

			FNLGuideLamp Lamp;
			Lamp.Seed = FMath::FRandRange(0.f, 100.f);
			if (LanternMesh)
			{
				Lamp.Mesh = NewObject<UStaticMeshComponent>(this);
				Lamp.Mesh->SetupAttachment(Root);
				Lamp.Mesh->SetMobility(EComponentMobility::Movable);
				Lamp.Mesh->SetStaticMesh(LanternMesh);
				Lamp.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Lamp.Mesh->SetCastShadow(false);
				Lamp.Mesh->RegisterComponent();
				Lamp.Mesh->SetWorldTransform(FTransform(Orient(D, FVector::UpVector), LanternAt));
				Lamp.Glass = Lamp.Mesh->CreateDynamicMaterialInstance(1);
				if (Lamp.Glass) Lamp.Glass->SetScalarParameterValue(TEXT("Intensity"), GlassGlow);
			}
			Lamp.Light = NewObject<UPointLightComponent>(this);
			Lamp.Light->SetupAttachment(Root);
			Lamp.Light->SetMobility(EComponentMobility::Movable);
			Lamp.Light->SetIntensityUnits(ELightUnits::Candelas);
			Lamp.Light->SetIntensity(LanternIntensity);
			Lamp.Light->SetAttenuationRadius(LanternRadius);
			Lamp.Light->SetLightColor(LampColor);
			Lamp.Light->SetCastShadows(false);
			Lamp.Light->RegisterComponent();
			Lamp.Light->SetWorldLocation(LanternAt - FVector(0, 0, 26.f));
			ScheduleFlicker(Lamp, Now, true);
			Lamps.Add(Lamp);

			if (!bSign)
			{
				// Tabela: ilk fenerin karşısında, yolun kenarında; levha yola paralel, yazı yola bakan yüzde
				bSign = true;
				const float S = -Side;
				float SOff = SideOffset * S * 0.8f;
				if (bMask && FindRoadEdges(P, N, Neg, Pos)) SOff = (S > 0.f ? Pos : Neg) + EdgeMargin * 0.6f * S;
				const FVector SB = Ground(P + N * SOff);
				Signs->AddInstance(FTransform(Orient(-N, FVector::UpVector), SB), true);
				const FVector Face = -N * S;
				SignLabel->SetWorldLocationAndRotation(SB + FVector(0, 0, 150.f) + Face * 4.f, Face.Rotation());
				SignLabel->SetText(FText::FromString(SignText));
				SignLabel->SetVisibility(true);
			}
			UE_LOG(LogTemp, Log, TEXT("NLGuide: lantern %d at (%.0f, %.0f, %.0f), road %.2f, offset %.0f"), Count, Base.X, Base.Y, Base.Z, PathAt(P + N * RoadMid), Off);
			++Count;
			Next += Spacing;
		}
		Walked += L;
	}
	UE_LOG(LogTemp, Log, TEXT("NLGuide: %d lanterns along %.0f m path (%s), %d placed beside the dirt road (mask %s)"),
		Count, Total / 100.f, bNav ? TEXT("navmesh") : TEXT("straight line"), OnRoad, bMask ? TEXT("ok") : TEXT("missing"));
}

void ANLGuideLanterns::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	for (FNLGuideLamp& L : Lamps)
	{
		// Normalde çok hafif dalgalanır (eski ampul / gaz lambası)
		float K = 0.92f + 0.08f * FMath::PerlinNoise1D(Now * 1.7f + L.Seed);
		if (Now >= L.NextFlicker)
		{
			if (Now >= L.FlickerEnd)
			{
				ScheduleFlicker(L, Now, false);
			}
			else
			{
				const float T = Now - L.NextFlicker;
				const float Dur = L.FlickerEnd - L.NextFlicker;
				// Hızlı, düzensiz yanıp sönme
				const float Buzz = FMath::PerlinNoise1D(T * 23.f + L.Seed * 3.f) * 0.5f + 0.5f;
				float F = Buzz > 0.45f ? FMath::Lerp(0.55f, 1.f, Buzz) : 0.05f;
				if (L.bBlackout)
				{
					// Önce cızırdar, sonra söner, sonunda yine cızırdayarak geri gelir
					const bool bDark = T > 0.35f && T < Dur - 0.45f;
					F = bDark ? 0.f : F;
				}
				K *= F;
			}
		}
		if (L.Light) L.Light->SetIntensity(LanternIntensity * K);
		if (L.Glass) L.Glass->SetScalarParameterValue(TEXT("Intensity"), GlassGlow * FMath::Max(K, 0.04f));
	}
}
