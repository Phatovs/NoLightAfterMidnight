#include "NLGuideLanterns.h"
#include "NLEnvKit.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

using namespace NLEnv;

ANLGuideLanterns::ANLGuideLanterns()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Posts = MakeISM(this, Root, TEXT("Posts"), true);
	Lanterns = MakeISM(this, Root, TEXT("Lanterns"), false);
	Signs = MakeISM(this, Root, TEXT("Signs"), true);
	for (UInstancedStaticMeshComponent* C : { Posts.Get(), Lanterns.Get(), Signs.Get() }) C->SetMobility(EComponentMobility::Movable);
	NL_KIT_MESH(Posts, "SM_NL_LanternPost");
	NL_KIT_MESH(Lanterns, "SM_NL_Lantern");
	NL_KIT_MESH(Signs, "SM_NL_Sign");

	SignLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignLabel"));
	SignLabel->SetupAttachment(Root);
	SignLabel->SetHorizontalAlignment(EHTA_Center);
	SignLabel->SetVerticalAlignment(EVRTA_TextCenter);
	SignLabel->SetWorldSize(13.f);
	SignLabel->SetTextRenderColor(FColor(205, 190, 160));
	SignLabel->SetVisibility(false);
}

void ANLGuideLanterns::Build(const FVector& Start, const FVector& Goal)
{
	UWorld* W = GetWorld();
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

	int32 Count = 0;
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
			const FVector Base = Ground(P + N * SideOffset * Side);
			const FVector Arm = -N * Side;
			Posts->AddInstance(FTransform(Orient(Arm, FVector::UpVector), Base), true);
			const FVector LanternAt = Base + Arm * 78.f + FVector(0, 0, 292.f);
			Lanterns->AddInstance(FTransform(Orient(D, FVector::UpVector), LanternAt), true);
			UPointLightComponent* L2 = NewObject<UPointLightComponent>(this);
			L2->SetupAttachment(Root);
			L2->SetMobility(EComponentMobility::Movable);
			L2->SetIntensityUnits(ELightUnits::Candelas);
			L2->SetIntensity(LanternIntensity);
			L2->SetAttenuationRadius(LanternRadius);
			L2->SetLightColor(LampColor);
			L2->SetCastShadows(false);
			L2->RegisterComponent();
			L2->SetWorldLocation(LanternAt - FVector(0, 0, 24.f));
			if (!bSign)
			{
				// Tabela: ilk fenerin karşısında, yolun yönünü gösterir; levha yaklaşan oyuncuya bakar
				bSign = true;
				// Tahta yola paralel (ok ucu yolun gidiş yönünde), yazı yola bakan yüzde
				const float S = -Side;
				const FVector SB = Ground(P + N * SideOffset * S * 0.8f);
				Signs->AddInstance(FTransform(Orient(-N, FVector::UpVector), SB), true);
				const FVector Face = -N * S;
				SignLabel->SetWorldLocationAndRotation(SB + FVector(0, 0, 150.f) + Face * 4.f, Face.Rotation());
				SignLabel->SetText(FText::FromString(SignText));
				SignLabel->SetVisibility(true);
			}
			UE_LOG(LogTemp, Log, TEXT("NLGuide: lantern %d at (%.0f, %.0f, %.0f), path dir yaw %.0f%s"), Count, Base.X, Base.Y, Base.Z, D.Rotation().Yaw, Count == 0 ? TEXT(", sign opposite") : TEXT(""));
			++Count;
			Next += Spacing;
		}
		Walked += L;
	}
	UE_LOG(LogTemp, Log, TEXT("NLGuide: %d lanterns along %.0f m path to the lighthouse (%s)"), Count, Total / 100.f, bNav ? TEXT("navmesh") : TEXT("straight line"));
}
