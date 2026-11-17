#include "NLSwingDoor.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ANLSwingDoor::ANLSwingDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	RootComponent = Hinge;
	Hinge->SetMobility(EComponentMobility::Movable);
	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(Hinge);
	Leaf->SetMobility(EComponentMobility::Movable);
	Leaf->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Leaf->SetCanEverAffectNavigation(false);
	Leaf->bAffectDistanceFieldLighting = false;
	Leaf->bVisibleInRayTracing = false;
	NL_DEFAULT_SOUND(OpenSound, "door_open");
	NL_DEFAULT_SOUND(CloseSound, "door_close");
}

void ANLSwingDoor::BeginPlay()
{
	Super::BeginPlay();
	SetOpen(bStartOpen, true);
}

void ANLSwingDoor::SetLeafMesh(UStaticMesh* Mesh)
{
	if (Leaf && Mesh) Leaf->SetStaticMesh(Mesh);
}

void ANLSwingDoor::SetOpen(bool bNewOpen, bool bInstant)
{
	if (bNewOpen != bOpen && !bInstant)
	{
		UGameplayStatics::PlaySoundAtLocation(this, bNewOpen ? OpenSound : CloseSound, Leaf->Bounds.Origin);
	}
	bOpen = bNewOpen;
	if (bInstant)
	{
		Alpha = bOpen ? 1.f : 0.f;
		Leaf->SetRelativeRotation(FRotator(0.f, OpenAngle * Alpha, 0.f));
	}
}

void ANLSwingDoor::Interact(ANLPlayerCharacter* Player)
{
	SetOpen(!bOpen);
}

FText ANLSwingDoor::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return FText::FromString(bOpen ? TEXT("[E] Close door") : TEXT("[E] Open door"));
}

void ANLSwingDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Target = bOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Alpha, Target))
	{
		Alpha = FMath::FInterpConstantTo(Alpha, Target, DeltaSeconds, 1.f / FMath::Max(SwingTime, 0.05f));
		// Yavaş başlayıp yavaş biten dönüş
		Leaf->SetRelativeRotation(FRotator(0.f, OpenAngle * FMath::SmoothStep(0.f, 1.f, Alpha), 0.f));
	}

	// Oyuncu olmayan karakterler için kendiliğinden aç
	NpcCheck -= DeltaSeconds;
	if (!bOpen && NpcCheck <= 0.f)
	{
		NpcCheck = 0.3f;
		const FVector C = Leaf->Bounds.Origin;
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			if (It->IsA<ANLPlayerCharacter>() || It->IsHidden()) continue;
			if (FVector::DistSquared(It->GetActorLocation(), C) < FMath::Square(AutoOpenRadius)) { SetOpen(true); break; }
		}
	}
}
