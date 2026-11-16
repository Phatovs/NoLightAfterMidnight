// NoLightAfterMidnight - Çevre dağıtımı: çam ağaçları, kıyı kayaları, eşyalar (HISM, uzakta otomatik gizlenir)
// Örnekleri editör scripti (Scripts/nl_dress_ada.py) ekler; elle de eklenebilir (bileşen > Instances).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLEnvScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLEnvScatter : public AActor
{
	GENERATED_BODY()

public:
	ANLEnvScatter();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PineA;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PineB;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PineC;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DeadPine;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockA;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockB;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RockC;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Barrels;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Crates;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Stumps;
};
