#include "NLGenerator.h"
#include "NLDefaults.h"
#include "NLPowerSubsystem.h"
#include "NLSaboteur.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

ANLGenerator::ANLGenerator()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(HumLoopSound, "generator_hum_loop");
	NL_DEFAULT_SOUND(EngineStartSound, "generator_start");
	NL_DEFAULT_SOUND(EngineStopSound, "generator_stop");
	NL_DEFAULT_SOUND(BreakdownSound, "generator_breakdown");
	NL_DEFAULT_SOUND(RepairHitSound, "repair_hit");
	NL_DEFAULT_SOUND(RepairMistakeSound, "repair_mistake");
	NL_DEFAULT_SOUND(RepairCompleteSound, "repair_complete");
	NL_DEFAULT_SOUND(RefuelSound, "refuel");

	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	HumAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudio"));
	HumAudio->SetupAttachment(Mesh);
	HumAudio->bAutoActivate = false;
}

void ANLGenerator::BeginPlay()
{
	Super::BeginPlay();

	Fuel = FMath::Clamp(StartFuel, 0.f, MaxFuel);
	ScheduleNextBreakdown();

	if (HumLoopSound)
	{
		HumAudio->SetSound(HumLoopSound);
	}

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RegisterGenerator(this);
	}
	NotifyPowerChanged(false);
}

void ANLGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Gün geçişinde / oyun bitince jeneratör donar
	const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	const bool bSimulate = !GM || GM->IsDayActive();

	const bool bWasProducing = IsProducingPower();

	if (bSimulate && bWasProducing)
	{
		Fuel = FMath::Max(0.f, Fuel - FuelDrainPerSecond * DeltaSeconds);
		if (Fuel <= 0.f)
		{
			OnFuelEmpty.Broadcast();
		}

		if (bRandomBreakdowns)
		{
			BreakdownTimer -= DeltaSeconds;
			if (BreakdownTimer <= 0.f)
			{
				// Şalter yeni attıysa / hâlâ atıksa şimdi bozulma; arıza aralığı dolunca biraz sonra dene
				float Wait = 0.f;
				UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
				if (Power && !Power->CanStartFailure(Wait))
				{
					BreakdownTimer = Wait + FMath::FRandRange(5.f, 20.f);
					UE_LOG(LogTemp, Log, TEXT("NLGenerator: breakdown postponed %.0f s (breaker failure active/recent)"), BreakdownTimer);
				}
				else
				{
					// Sabotajcı gelip jeneratörü bozar; gelemezse hemen boz
					BreakdownTimer = 1.0e9f;
					TWeakObjectPtr<ANLGenerator> WeakThis(this);
					if (!ANLSaboteur::RequestSabotage(GetWorld(), this, [WeakThis]() { if (WeakThis.IsValid()) WeakThis->BreakDown(); }))
					{
						BreakDown();
					}
				}
			}
		}
	}

	// Tamir ibresi ileri-geri gider
	if (IsBeingRepaired())
	{
		NeedlePos += NeedleDir * NeedleSpeed * DeltaSeconds;
		if (NeedlePos >= 1.f) { NeedlePos = 1.f; NeedleDir = -1.f; }
		if (NeedlePos <= 0.f) { NeedlePos = 0.f; NeedleDir = 1.f; }
	}

	// Bozukken diğer arızalar (şalter) bekler
	if (bBroken)
	{
		if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	}

	if (bWasProducing != IsProducingPower())
	{
		NotifyPowerChanged(true);
	}
}

void ANLGenerator::Interact(ANLPlayerCharacter* Player)
{
	if (!Player) return;

	if (bBroken && !IsBeingRepaired())
	{
		StartRepair(Player);
		return;
	}

	if (Player->HasFuelCan() && Fuel < MaxFuel)
	{
		// Oyuncu bidonu kaldırıp jeneratörün üstüne döker; benzin dökülmenin ortasında eklenir
		UGameplayStatics::PlaySoundAtLocation(this, RefuelSound, GetActorLocation());
		Player->PlayBodyMontage(PlayerRefuelMontage);

		const FVector Pour = GetPourPoint(Player->GetActorLocation());
		const FRotator Facing(0.f, (GetComponentsBoundingBox().GetCenter() - Player->GetActorLocation()).Rotation().Yaw, 0.f);
		TWeakObjectPtr<ANLGenerator> WeakThis(this);
		TWeakObjectPtr<ANLPlayerCharacter> WeakPlayer(Player);
		Player->PlayHandAction(ENLHandAction::Refuel, Pour, Facing, RefuelActionDuration, 0.6f, [WeakThis, WeakPlayer]()
		{
			ANLGenerator* Gen = WeakThis.Get();
			ANLPlayerCharacter* P = WeakPlayer.Get();
			if (!Gen || !P) return;
			const bool bWasProducing = Gen->IsProducingPower();
			Gen->AddFuel(P->ConsumeFuelCan());
			if (bWasProducing != Gen->IsProducingPower())
			{
				Gen->NotifyPowerChanged(true);
			}
		});
	}
}

FVector ANLGenerator::GetPourPoint(const FVector& From) const
{
	// Jeneratörün üstünde, oyuncuya bakan kenarın biraz içinde; bidon kulpu bu yüksekliğe kalkar
	const FBox Box = GetComponentsBoundingBox();
	FVector P = Box.GetCenter();
	FVector ToPlayer = From - P;
	ToPlayer.Z = 0.f;
	const float Edge = FMath::Min(Box.GetExtent().X, Box.GetExtent().Y);
	P += ToPlayer.GetSafeNormal() * FMath::Max(Edge - 10.f, 0.f);
	P.Z = Box.Max.Z + 28.f;
	return P;
}

FText ANLGenerator::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	if (bBroken)
	{
		return IsBeingRepaired()
			? FText::FromString(TEXT("[SPACE] Repair   [E] Stop"))
			: FText::FromString(TEXT("[E] Repair generator"));
	}
	if (Player && Player->HasFuelCan())
	{
		return FText::FromString(TEXT("[E] Refuel"));
	}
	return FText::FromString(FString::Printf(TEXT("Generator - Fuel: %d%%"), FMath::RoundToInt(GetFuelPercent() * 100.f)));
}

void ANLGenerator::AddFuel(float Amount)
{
	Fuel = FMath::Clamp(Fuel + Amount, 0.f, MaxFuel);
}

void ANLGenerator::BreakDown()
{
	if (bBroken) return;

	bBroken = true;
	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	UE_LOG(LogTemp, Log, TEXT("NLGenerator: broke down (game time %.0f s)"), GetWorld()->GetTimeSeconds());
	UGameplayStatics::PlaySoundAtLocation(this, BreakdownSound, GetActorLocation());
	OnBrokeDown.Broadcast();
	NotifyPowerChanged(true);
}

void ANLGenerator::StartRepair(ANLPlayerCharacter* Player)
{
	RepairingPlayer = Player;
	RepairHits = 0;
	NeedlePos = 0.f;
	NeedleDir = 1.f;
	RandomizeZone();
	Player->EnterRepairMode(this, PlayerRepairMontage);
}

void ANLGenerator::CancelRepair()
{
	if (ANLPlayerCharacter* Player = RepairingPlayer.Get())
	{
		RepairingPlayer.Reset();
		Player->ExitRepairMode();
	}
}

void ANLGenerator::HandleRepairInput()
{
	if (!IsBeingRepaired()) return;

	const bool bInZone = NeedlePos >= ZoneStart && NeedlePos <= ZoneStart + ZoneWidth;

	if (bInZone)
	{
		++RepairHits;
		UGameplayStatics::PlaySoundAtLocation(this, RepairHitSound, GetActorLocation());

		if (RepairHits >= RequiredRepairHits)
		{
			FinishRepair();
			return;
		}
	}
	else
	{
		// HATA: Runner gelir
		UGameplayStatics::PlaySoundAtLocation(this, RepairMistakeSound, GetActorLocation());
		OnRepairMistake.Broadcast();

		if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
		{
			GM->RequestRunnerChase(RepairingPlayer.Get());
		}
	}

	RandomizeZone();
}

void ANLGenerator::FinishRepair()
{
	bBroken = false;
	RepairHits = 0;
	ScheduleNextBreakdown();
	UGameplayStatics::PlaySoundAtLocation(this, RepairCompleteSound, GetActorLocation());

	CancelRepair(); // oyuncuyu tamir modundan çıkarır
	OnRepaired.Broadcast();
	NotifyPowerChanged(true);
}

void ANLGenerator::ScheduleNextBreakdown()
{
	BreakdownTimer = FMath::FRandRange(MinBreakdownInterval, FMath::Max(MinBreakdownInterval, MaxBreakdownInterval));
}

void ANLGenerator::RandomizeZone()
{
	ZoneStart = FMath::FRandRange(0.05f, FMath::Max(0.05f, 0.95f - ZoneWidth));
}

void ANLGenerator::NotifyPowerChanged(bool bPlayTransitionSounds)
{
	// Motor sesi
	if (IsProducingPower())
	{
		if (!HumAudio->IsPlaying())
		{
			if (bPlayTransitionSounds) UGameplayStatics::PlaySoundAtLocation(this, EngineStartSound, GetActorLocation());
			HumAudio->Play();
		}
	}
	else if (HumAudio->IsPlaying())
	{
		HumAudio->Stop();
		if (bPlayTransitionSounds) UGameplayStatics::PlaySoundAtLocation(this, EngineStopSound, GetActorLocation());
	}

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RefreshPower();
	}
}
