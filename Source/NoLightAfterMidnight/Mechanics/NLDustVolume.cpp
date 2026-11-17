#include "NLDustVolume.h"
#include "NLEnvKit.h"
#include "Components/BoxComponent.h"

ANLDustVolume::ANLDustVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetHiddenInGame(true);
	Box->SetMobility(EComponentMobility::Static);

	Motes = NLEnv::MakeISM(this, Box, TEXT("Motes"), false);
	NL_KIT_MESH(Motes, "SM_NL_Dust");
	Motes->SetCastShadow(false);
	// Hareketli küçük obje: uzaklık alanı / Lumen sahnesine girmesin (her kare güncelleme maliyeti)
	Motes->bAffectDistanceFieldLighting = false;
	Motes->bAffectDynamicIndirectLighting = false;
	Motes->bVisibleInRayTracing = false;
	Motes->SetCullDistances(0, 2500);
	// Zerreler materyalde ~25 cm salınır: sınırlar biraz geniş olsun
	Motes->SetBoundsScale(1.3f);
}

void ANLDustVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANLDustVolume::Rebuild()
{
	Box->SetBoxExtent(Extent);
	Motes->ClearInstances();
	const float M3 = 8.f * Extent.X * Extent.Y * Extent.Z / 1.0e6f;
	const int32 Count = FMath::Clamp(FMath::RoundToInt(M3 * Density), 8, 4000);
	FRandomStream Rng(Seed);
	TArray<FTransform> T;
	T.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector P(Rng.FRandRange(-Extent.X, Extent.X), Rng.FRandRange(-Extent.Y, Extent.Y), Rng.FRandRange(-Extent.Z, Extent.Z));
		T.Add(FTransform(FQuat::Identity, P, FVector(MoteScale * Rng.FRandRange(0.6f, 1.7f))));
	}
	Motes->AddInstances(T, false);
}
