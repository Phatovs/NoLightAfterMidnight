#include "NLSeatedFigure.h"
#include "NLBodyRigComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ANLSeatedFigure::ANLSeatedFigure()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> MeshFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));

	// Görünmez sürücü: boşta duruş animasyonu (gövde bunun üstüne pozlanır)
	Driver = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Driver"));
	Driver->SetupAttachment(Root);
	Driver->SetSkeletalMeshAsset(MeshFinder.Get());
	if (AnimFinder.Succeeded())
	{
		Driver->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Driver->SetAnimInstanceClass(AnimFinder.Class);
	}
	Driver->SetHiddenInGame(true);
	Driver->SetCastShadow(false);
	Driver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Body = CreateDefaultSubobject<UNLBodyRigComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->bHideHead = false;
	Body->SetCastShadow(true);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Mesh ayakta dururken kalçası ~95 cm'de: oturak hizasına göre aşağı al, yüzü +X'e baksın
	for (USceneComponent* C : { static_cast<USceneComponent*>(Driver.Get()), static_cast<USceneComponent*>(Body.Get()) })
	{
		C->SetRelativeLocationAndRotation(FVector(-8.f, 0.f, -FloorDrop), FRotator(0.f, -90.f, 0.f));
	}
}

void ANLSeatedFigure::BeginPlay()
{
	Super::BeginPlay();
	if (!bSeated)
	{
		// Ayakta: orijin kalçada, ayaklar zeminde
		for (USceneComponent* C : { static_cast<USceneComponent*>(Driver.Get()), static_cast<USceneComponent*>(Body.Get()) })
		{
			C->SetRelativeLocation(FVector(0.f, 0.f, -StandHipHeight));
		}
	}
	if (USkeletalMesh* M = Driver->GetSkeletalMeshAsset()) Body->SetSkinnedAssetAndUpdate(M);
	Body->SetDriver(Driver);
	SetClothing(Clothing);
	FNLBodyPoseInput& In = Body->GetPoseInput();
	In.bHoldFlashlight = false;
	In.IdleArmHold = 1.f;
}

void ANLSeatedFigure::SetClothing(UMaterialInterface* M)
{
	Clothing = M;
	if (!M || !Body) return;
	for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, M);
}

void ANLSeatedFigure::SetHandTargetsWorld(float AlphaR, const FVector& R, float AlphaL, const FVector& L)
{
	bWorldTargets = true;
	WorldAlphaR = FMath::Clamp(AlphaR, 0.f, 1.f);
	WorldAlphaL = FMath::Clamp(AlphaL, 0.f, 1.f);
	WorldR = R;
	WorldL = L;
}

void ANLSeatedFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FTransform X = GetActorTransform();
	const FVector Fwd = X.GetUnitAxis(EAxis::X);
	const FVector Right = X.GetUnitAxis(EAxis::Y);
	const FVector Up = X.GetUnitAxis(EAxis::Z);

	FNLBodyPoseInput& In = Body->GetPoseInput();
	if (bSeated)
	{
		In.SitAlpha = 1.f;
		In.SitPelvis = X.GetLocation() + Up * 9.f - Fwd * 4.f;
		In.SitForward = Fwd;
		In.SitFloorZ = X.GetLocation().Z - FloorDrop;
		In.SitDeskHeight = FloorDrop + 20.f;
		In.View = FTransform((X.GetRotation() * LookRotation.Quaternion()), X.GetLocation() + Up * 72.f + Fwd * 6.f);
		In.ReachAlphaR = bReachRight ? 1.f : 0.f;
		In.ReachTargetR = X.TransformPosition(RightHandTarget);
		// Sol el dizinin üstünde
		In.ReachAlphaL = 1.f;
		In.ReachTargetL = X.GetLocation() + Fwd * 30.f - Right * 13.f + Up * 14.f;
	}
	else
	{
		In.SitAlpha = 0.f;
		In.View = FTransform((X.GetRotation() * LookRotation.Quaternion()), X.GetLocation() + Up * 70.f + Fwd * 8.f);
		In.ReachAlphaR = bWorldTargets ? WorldAlphaR : 0.f;
		In.ReachTargetR = WorldR;
		In.ReachAlphaL = bWorldTargets ? WorldAlphaL : 0.f;
		In.ReachTargetL = WorldL;
	}

	// El sallama: sağ el başın yanında, iki yana
	if (WaveAlpha > KINDA_SMALL_NUMBER)
	{
		WaveTime += DeltaSeconds;
		const FVector Wave = X.GetLocation() + Up * 78.f + Fwd * 22.f + Right * (30.f + 14.f * FMath::Sin(WaveTime * 8.f));
		In.ReachTargetR = FMath::Lerp(In.ReachAlphaR > 0.f ? In.ReachTargetR : Wave, Wave, WaveAlpha);
		In.ReachAlphaR = FMath::Max(In.ReachAlphaR, WaveAlpha);
	}
	else
	{
		WaveTime = 0.f;
	}
}
