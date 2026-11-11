#include "NLFuseBox.h"
#include "NLDefaults.h"
#include "NLPowerSubsystem.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ANLFuseBox::ANLFuseBox()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(SwitchOnSound, "fuse_switch_on");
	NL_DEFAULT_SOUND(SwitchOffSound, "fuse_switch_off");
	NL_DEFAULT_SOUND(TripSound, "fuse_trip");

	PrimaryActorTick.bCanEverTick = true;

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
	ScheduleNextTrip();

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RegisterFuseBox(this);
	}
}

void ANLFuseBox::Interact(ANLPlayerCharacter* Player)
{
	if (!Player)
	{
		SetOn(!bOn);
		return;
	}

	// Oyuncu kolu tutup çeker: açıkken aşağı, kapalıyken yukarı. Kol, çekildiği anda döner.
	Player->PlayBodyMontage(PlayerSwitchMontage);
	const FVector Handle = LeverMesh->GetStaticMesh() ? LeverMesh->Bounds.Origin : GetComponentsBoundingBox().GetCenter();
	TWeakObjectPtr<ANLFuseBox> WeakThis(this);
	const bool bTurnOn = !bOn;
	Player->PlayHandAction(ENLHandAction::Switch, Handle, FRotator::ZeroRotator, SwitchActionDuration, 0.5f, [WeakThis, bTurnOn]()
	{
		if (ANLFuseBox* Self = WeakThis.Get()) Self->SetOn(bTurnOn);
	}, bTurnOn ? 1.f : -1.f);
}

FText ANLFuseBox::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return bOn ? FText::FromString(TEXT("[E] Switch power off"))
	           : FText::FromString(TEXT("[E] Switch power on"));
}

void ANLFuseBox::SetOn(bool bNewOn)
{
	if (bOn == bNewOn) return;

	bOn = bNewOn;
	if (bOn)
	{
		// Tekrar açıldı: sayaç baştan başlar
		bTripped = false;
		ScheduleNextTrip();
	}
	if (bOn || !bTripped)
	{
		UGameplayStatics::PlaySoundAtLocation(this, bOn ? SwitchOnSound : SwitchOffSound, GetActorLocation());
	}
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
	bTripped = true;
	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	UE_LOG(LogTemp, Log, TEXT("NLFuseBox: breaker tripped by itself (game time %.0f s)"), GetWorld()->GetTimeSeconds());
	UGameplayStatics::PlaySoundAtLocation(this, TripSound, GetActorLocation());
	SetOn(false);
}

void ANLFuseBox::ApplyVisuals()
{
	// Oyun başında anında yerine koy; sonrasında Tick yumuşakça döndürür
	if (!HasActorBegunPlay())
	{
		LeverAngle = bOn ? LeverOnAngle : LeverOffAngle;
	}
	PlaceLever(LeverAngle);
}

void ANLFuseBox::PlaceLever(float AngleDeg)
{
	// Kol mafsaldan dışarı çıkar ve yukarı (açık) / aşağı (kapalı) bakar
	const FVector Out = LeverOutward.GetSafeNormal().IsNearlyZero() ? FVector(0.f, 1.f, 0.f) : LeverOutward.GetSafeNormal();
	const float Rad = FMath::DegreesToRadians(AngleDeg);
	const FVector Dir = (Out * FMath::Cos(Rad) + FVector::UpVector * FMath::Sin(Rad)).GetSafeNormal();
	const FQuat Rot = FQuat::FindBetweenNormals(FVector::UpVector, Dir);  // kol mesh'inin uzun ekseni Z
	LeverMesh->SetRelativeLocationAndRotation(LeverPivot + Dir * (LeverLength * 0.5f), Rot);
}

void ANLFuseBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Kol hedef konuma döner (atınca aşağı düştüğü görülsün)
	const float TargetAngle = bOn ? LeverOnAngle : LeverOffAngle;
	if (!FMath::IsNearlyEqual(LeverAngle, TargetAngle))
	{
		LeverAngle = FMath::FInterpConstantTo(LeverAngle, TargetAngle, DeltaSeconds, LeverSpeed);
		PlaceLever(LeverAngle);
	}

	// Atıkken diğer arızalar (jeneratör) bekler
	if (bTripped && !bOn)
	{
		if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	}

	if (!bRandomTrips || !bOn) return;

	// Sadece gün sürerken sayar (gün geçişinde / oyun bitince atmaz)
	const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	if (GM && !GM->IsDayActive()) return;

	TripTimer -= DeltaSeconds;
	if (TripTimer <= 0.f)
	{
		// Jeneratör bozuksa / yeni bozulduysa şimdi atma; arıza aralığı dolunca biraz sonra dene
		float Wait = 0.f;
		UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
		if (Power && !Power->CanStartFailure(Wait))
		{
			TripTimer = Wait + FMath::FRandRange(5.f, 20.f);
			UE_LOG(LogTemp, Log, TEXT("NLFuseBox: trip postponed %.0f s (generator failure active/recent)"), TripTimer);
		}
		else
		{
			Trip();
		}
	}
}

void ANLFuseBox::ScheduleNextTrip()
{
	TripTimer = FMath::FRandRange(MinTripInterval, FMath::Max(MinTripInterval, MaxTripInterval));
}
