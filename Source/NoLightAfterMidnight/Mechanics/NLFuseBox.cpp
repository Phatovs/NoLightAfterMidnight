#include "NLFuseBox.h"
#include "NLDefaults.h"
#include "NLPowerSubsystem.h"
#include "NLPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ANLFuseBox::ANLFuseBox()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(SwitchOnSound, "fuse_switch_on");
	NL_DEFAULT_SOUND(SwitchOffSound, "fuse_switch_off");
	NL_DEFAULT_SOUND(TripSound, "fuse_switch_off");

	PrimaryActorTick.bCanEverTick = false;

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = BoxMesh;

	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	LeverMesh->SetupAttachment(BoxMesh);
}

void ANLFuseBox::BeginPlay()
{
	Super::BeginPlay();
	bOn = bStartOn;
	ApplyVisuals();

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RegisterFuseBox(this);
	}
}

void ANLFuseBox::Interact(ANLPlayerCharacter* Player)
{
	if (Player) Player->PlayBodyMontage(PlayerSwitchMontage);
	SetOn(!bOn);
}

FText ANLFuseBox::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return bOn ? FText::FromString(TEXT("[E] Şalteri kapat"))
	           : FText::FromString(TEXT("[E] Şalteri aç"));
}

void ANLFuseBox::SetOn(bool bNewOn)
{
	if (bOn == bNewOn) return;

	bOn = bNewOn;
	UGameplayStatics::PlaySoundAtLocation(this, bOn ? SwitchOnSound : SwitchOffSound, GetActorLocation());
	ApplyVisuals();
	OnSwitched.Broadcast(bOn);

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RefreshPower();
	}
}

void ANLFuseBox::Trip()
{
	if (!bOn) return;
	UGameplayStatics::PlaySoundAtLocation(this, TripSound, GetActorLocation());
	SetOn(false);
}

void ANLFuseBox::ApplyVisuals()
{
	LeverMesh->SetRelativeRotation(bOn ? LeverOnRotation : LeverOffRotation);
}
