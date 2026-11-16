#include "NLEnvScatter.h"
#include "NLEnvKit.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

using namespace NLEnv;

ANLEnvScatter::ANLEnvScatter()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Static);

	using H = UHierarchicalInstancedStaticMeshComponent;
	PineA = MakeISM<H>(this, Root, TEXT("PineA"), true);
	PineB = MakeISM<H>(this, Root, TEXT("PineB"), true);
	PineC = MakeISM<H>(this, Root, TEXT("PineC"), true);
	DeadPine = MakeISM<H>(this, Root, TEXT("DeadPine"), true);
	RockA = MakeISM<H>(this, Root, TEXT("RockA"), true);
	RockB = MakeISM<H>(this, Root, TEXT("RockB"), true);
	RockC = MakeISM<H>(this, Root, TEXT("RockC"), true);
	Barrels = MakeISM<H>(this, Root, TEXT("Barrels"), true);
	Crates = MakeISM<H>(this, Root, TEXT("Crates"), true);
	Stumps = MakeISM<H>(this, Root, TEXT("Stumps"), true);
	NL_KIT_MESH(PineA, "SM_NL_PineA");
	NL_KIT_MESH(PineB, "SM_NL_PineB");
	NL_KIT_MESH(PineC, "SM_NL_PineC");
	NL_KIT_MESH(DeadPine, "SM_NL_DeadPine");
	NL_KIT_MESH(RockA, "SM_NL_RockA");
	NL_KIT_MESH(RockB, "SM_NL_RockB");
	NL_KIT_MESH(RockC, "SM_NL_RockC");
	NL_KIT_MESH(Barrels, "SM_NL_Barrel");
	NL_KIT_MESH(Crates, "SM_NL_Crate");
	NL_KIT_MESH(Stumps, "SM_NL_Stump");

	// Sis 60-80 m'den ötesini tamamen yutar; ağaçlar 90 m'den sonra çizilmez, küçük şeyler daha erken
	for (H* C : { PineA.Get(), PineB.Get(), PineC.Get(), DeadPine.Get() }) C->SetCullDistances(0, 9000);
	for (H* C : { RockA.Get(), RockB.Get(), RockC.Get() }) C->SetCullDistances(0, 8000);
	for (H* C : { Barrels.Get(), Crates.Get(), Stumps.Get() }) C->SetCullDistances(0, 4500);
	// Performans: kayalar ve küçük eşyalar ay gölgesi atmaz (bu karanlıkta görünmüyor)
	for (H* C : { RockA.Get(), RockB.Get(), RockC.Get(), Barrels.Get(), Crates.Get(), Stumps.Get() }) C->SetCastShadow(false);
}
