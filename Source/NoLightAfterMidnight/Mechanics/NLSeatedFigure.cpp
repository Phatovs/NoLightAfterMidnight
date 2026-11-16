#include "NLSeatedFigure.h"
#include "NLBodyRigComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
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
	if (USkeletalMesh* M = Driver->GetSkeletalMeshAsset()) Body->SetSkinnedAssetAndUpdate(M);
	Body->SetDriver(Driver);
	FNLBodyPoseInput& In = Body->GetPoseInput();
	In.bHoldFlashlight = false;
	In.IdleArmHold = 1.f;
}

void ANLSeatedFigure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FTransform X = GetActorTransform();
	const FVector Fwd = X.GetUnitAxis(EAxis::X);
	const FVector Right = X.GetUnitAxis(EAxis::Y);
	const FVector Up = X.GetUnitAxis(EAxis::Z);

	FNLBodyPoseInput& In = Body->GetPoseInput();
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
