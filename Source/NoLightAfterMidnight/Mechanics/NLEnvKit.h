// NoLightAfterMidnight - Çevre kiti: ortak mesh yolları ve instanced yerleştirme yardımcıları
// Kit meshleri Scripts/nl_env_kit.py ile üretilir (/Game/Environment/Kit). Ölçü kuralları:
//   SM_NL_Log        X boyunca 100 cm, çap 30, ortada        -> ölçek X = uzunluk/100, YZ = çap/30
//   SM_NL_Board      100 (X) x 20 (Y) x 4 (Z), ortada
//   SM_NL_ShingleRow X: -50..50, Y: 0..40 (aşağı doğru), üst kenar orijinde
//   SM_NL_DeckPanel  200 (X) x 100 (Y), üst yüzey z=0
//   SM_NL_Pane       100 (X) x 1 (Y) x 100 (Z), ortada (cam)
//   SM_NL_Lantern    asma halkası orijinde, aşağı ~40 cm
//   SM_NL_Stovepipe / SM_NL_Post / SM_NL_Barrel / SM_NL_Crate / SM_NL_RopeCoil / SM_NL_LanternPost: taban z=0
#pragma once

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

// Kurucu içinde: kit meshini bileşene ata (asset yoksa boş kalır, oyun bozulmaz)
#define NL_KIT_MESH(Comp, AssetName) \
	{ \
		static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> NLKitFinder(TEXT("/Game/Environment/Kit/" AssetName "." AssetName)); \
		if (NLKitFinder.Get()) (Comp)->SetStaticMesh(NLKitFinder.Get()); \
	}

namespace NLEnv
{
	// X ekseni XDir, Z ekseni ZHint'e en yakın olacak şekilde döndürme
	inline FQuat Orient(const FVector& XDir, const FVector& ZHint)
	{
		return FRotationMatrix::MakeFromXZ(XDir.GetSafeNormal(), ZHint.GetSafeNormal()).ToQuat();
	}

	inline void Add(UInstancedStaticMeshComponent* C, const FVector& Loc, const FQuat& Rot, const FVector& Scale)
	{
		if (C && C->GetStaticMesh()) C->AddInstance(FTransform(Rot, Loc, Scale), false);
	}

	template <class T = UInstancedStaticMeshComponent>
	T* MakeISM(UObject* Owner, USceneComponent* Parent, FName Name, bool bCollision)
	{
		T* C = Owner->CreateDefaultSubobject<T>(Name);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Static);
		C->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCollision) C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->SetCanEverAffectNavigation(bCollision);
		return C;
	}
}
