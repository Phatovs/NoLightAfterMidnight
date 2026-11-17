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
	NL_KIT_MESH(Posts, "SM_NL_GuidePost");
	NL_KIT_MESH(Signs, "SM_NL_Sign");
	{
		static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> LanternFinder(TEXT("/Game/Environment/Kit/SM_NL_GuideLantern.SM_NL_GuideLantern"));
		static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> FlameFinder(TEXT("/Game/Environment/Kit/SM_NL_LanternFlame.SM_NL_LanternFlame"));
		LanternMesh = LanternFinder.Get();
		FlameMesh = FlameFinder.Get();
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
			// Fener kancası: kolun ucu (SM_NL_GuidePost: X 55, Z 220)
			const FVector LanternAt = Base + Arm * 55.f + FVector(0, 0, 220.f * PostHeightScale);

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
				Lamp.Mesh->bVisibleInRayTracing = false;
				Lamp.Mesh->RegisterComponent();
				Lamp.Mesh->SetWorldTransform(FTransform(Orient(D, FVector::UpVector), LanternAt));
				const int32 GlassSlot = Lamp.Mesh->GetMaterialIndex(TEXT("MI_NL_LanternGlass"));
				Lamp.Glass = Lamp.Mesh->CreateDynamicMaterialInstance(GlassSlot == INDEX_NONE ? 1 : GlassSlot);
				if (Lamp.Glass) Lamp.Glass->SetScalarParameterValue(TEXT("Intensity"), GlassGlow);
				// Mum alevi (fenerin içinde, fenerle birlikte sallanır)
				if (FlameMesh)
				{
					Lamp.Flame = NewObject<UStaticMeshComponent>(this);
					Lamp.Flame->SetupAttachment(Lamp.Mesh);
					Lamp.Flame->SetMobility(EComponentMobility::Movable);
					Lamp.Flame->SetStaticMesh(FlameMesh);
					Lamp.Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Lamp.Flame->SetCastShadow(false);
					Lamp.Flame->bAffectDistanceFieldLighting = false;
					Lamp.Flame->bAffectDynamicIndirectLighting = false;
					Lamp.Flame->bVisibleInRayTracing = false;
					Lamp.Flame->RegisterComponent();
					Lamp.Flame->SetRelativeLocation(FVector(0.f, 0.f, -29.f));
				}
			}
			Lamp.Light = NewObject<UPointLightComponent>(this);
			// Işık fenerin içinde, fenerle birlikte sallanır
			Lamp.Light->SetupAttachment(Lamp.Mesh ? static_cast<USceneComponent*>(Lamp.Mesh) : static_cast<USceneComponent*>(Root));
			Lamp.Light->SetMobility(EComponentMobility::Movable);
			Lamp.Light->SetIntensityUnits(ELightUnits::Candelas);
			Lamp.Light->SetIntensity(LanternIntensity);
			Lamp.Light->SetAttenuationRadius(LanternRadius);
			Lamp.Light->SetLightColor(LampColor);
			Lamp.Light->SetCastShadows(false);
			Lamp.Light->RegisterComponent();
			Lamp.Light->SetWorldLocation(LanternAt - FVector(0, 0, 25.f));
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
	const float T = GetWorld()->GetTimeSeconds();
	for (FNLGuideLamp& L : Lamps)
	{
		// Mum gibi: birkaç hızda yumuşak dalgalanma, hiç sönmez
		const float S = L.Seed;
		const float Wave = 0.10f * FMath::PerlinNoise1D(T * 1.9f + S) + 0.06f * FMath::PerlinNoise1D(T * 5.3f + S * 1.7f) + 0.03f * FMath::PerlinNoise1D(T * 13.f + S * 2.3f);
		const float K = 0.86f + Wave * FlickerStrength;
		if (L.Light) L.Light->SetIntensity(LanternIntensity * K);
		if (L.Glass) L.Glass->SetScalarParameterValue(TEXT("Intensity"), GlassGlow * (0.9f + 1.4f * Wave * FlickerStrength));
		// Alev boy atar ve hafifçe yana yatar
		if (L.Flame)
		{
			const float H = 1.f + 0.16f * FMath::PerlinNoise1D(T * 6.f + S * 3.1f) + 0.06f * FMath::PerlinNoise1D(T * 15.f + S);
			const float W = 1.f + 0.07f * FMath::PerlinNoise1D(T * 9.f + S * 0.7f);
			L.Flame->SetRelativeScale3D(FVector(W, W, H));
			L.Flame->SetRelativeRotation(FRotator(5.f * FMath::PerlinNoise1D(T * 3.f + S), 0.f, 5.f * FMath::PerlinNoise1D(T * 3.4f + S * 1.3f)));
		}
		// Fener rüzgârda kancasında hafifçe sallanır
		if (L.Mesh)
		{
			const float Yaw = L.Mesh->GetComponentRotation().Yaw;
			L.Mesh->SetWorldRotation(FRotator(2.5f * FMath::PerlinNoise1D(T * 0.7f + S), Yaw, 2.5f * FMath::PerlinNoise1D(T * 0.6f + S * 2.f)));
		}
	}
}
