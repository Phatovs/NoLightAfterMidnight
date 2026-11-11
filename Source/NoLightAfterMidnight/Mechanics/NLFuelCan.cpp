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

bool ANLFuelCan::CanInteract(const ANLPlayerCharacter* Player) const
{
	return bAvailable && Player && !Player->HasFuelCan();
}

void ANLFuelCan::Interact(ANLPlayerCharacter* Player)
{
	if (!CanInteract(Player)) return;

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
	SetAvailable(true);
}
