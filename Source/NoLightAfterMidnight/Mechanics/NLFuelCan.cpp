#include "NLFuelCan.h"
#include "NLDefaults.h"
#include "NLPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ANLFuelCan::ANLFuelCan()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(PickupSound, "fuel_pickup");

	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
}

void ANLFuelCan::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();
	DefaultFuelAmount = FuelAmount;
}

void ANLFuelCan::DropAt(const FVector& GroundLocation, float Yaw, float Amount)
{
	ANLFuelCan* Target = this;
	if (bAvailable)
	{
		// Bu bidon kulübede yeniden belirmiş: yere bırakılan için ayrı bir bidon
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Target = GetWorld()->SpawnActor<ANLFuelCan>(GetClass(), FTransform(FRotator(0.f, Yaw, 0.f), GroundLocation), SP);
		if (!Target) return;
		Target->Mesh->SetStaticMesh(Mesh->GetStaticMesh());
		for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i) Target->Mesh->SetMaterial(i, Mesh->GetMaterial(i));
		Target->RespawnDelay = 0.f;
		Target->bDroppedCopy = true;
	}
	Target->GetWorldTimerManager().ClearTimer(Target->RespawnTimer);
	Target->SetActorLocationAndRotation(GroundLocation, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Target->FuelAmount = Amount;
	Target->SetAvailable(true);
	UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GroundLocation, 0.7f, 0.8f);
	UE_LOG(LogTemp, Log, TEXT("NLFuelCan: dropped at (%.0f, %.0f, %.0f) with %.0f fuel%s"), GroundLocation.X, GroundLocation.Y, GroundLocation.Z, Amount, Target != this ? TEXT(" (new can)") : TEXT(""));
}

bool ANLFuelCan::CanInteract(const ANLPlayerCharacter* Player) const
{
	return bAvailable && Player && !Player->HasFuelCan();
}

void ANLFuelCan::Interact(ANLPlayerCharacter* Player)
{
	if (!CanInteract(Player)) return;

	Player->SetCanSource(this);
	Player->PickupFuelCan(FuelAmount, GetComponentsBoundingBox().GetCenter() + FVector(0.f, 0.f, GetComponentsBoundingBox().GetExtent().Z));
	Player->PlayBodyMontage(PlayerPickupMontage);
	UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	SetAvailable(false);

	if (RespawnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &ANLFuelCan::Respawn, RespawnDelay, false);
	}
}

FText ANLFuelCan::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	if (!bAvailable) return FText::GetEmpty();
	if (Player && Player->HasFuelCan()) return FText::FromString(TEXT("You are already carrying a fuel can"));
	return FText::FromString(TEXT("[E] Pick up fuel can"));
}

void ANLFuelCan::SetAvailable(bool bNewAvailable)
{
	bAvailable = bNewAvailable;
	SetActorHiddenInGame(!bAvailable);
	SetActorEnableCollision(bAvailable);
}

void ANLFuelCan::Respawn()
{
	// Yere bırakılıp yeniden alınan bidon da kulübedeki yerinde belirir
	SetActorTransform(HomeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	FuelAmount = DefaultFuelAmount;
	SetAvailable(true);
}
