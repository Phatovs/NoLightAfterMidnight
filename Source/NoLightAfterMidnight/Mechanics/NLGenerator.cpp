#include "NLGenerator.h"
#include "NLDefaults.h"
#include "NLPowerSubsystem.h"
#include "NLSaboteur.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"

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
	// Eski tip açık motorlu jeneratör (Scripts/blender_old_generator.py); levelde başka mesh verilmişse o kalır
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> GenFinder(TEXT("/Game/Environment/Kit/SM_NL_OldGenerator.SM_NL_OldGenerator"));
	if (GenFinder.Get()) Mesh->SetStaticMesh(GenFinder.Get());
	LampLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LampLight"));
	LampLight->SetupAttachment(Mesh);
	LampLight->SetRelativeLocation(FVector(-50.f, -78.f, 82.f));
	LampLight->SetIntensityUnits(ELightUnits::Candelas);
	LampLight->SetIntensity(0.f);
	LampLight->SetAttenuationRadius(170.f);
	LampLight->SetCastShadows(false);

	HumAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("HumAudio"));
	HumAudio->SetupAttachment(Mesh);
	HumAudio->bAutoActivate = false;
}

void ANLGenerator::BeginPlay()
{
	Super::BeginPlay();
	const int32 LampSlot = Mesh->GetMaterialIndex(TEXT("MI_NL_GenLamp"));
	if (LampSlot != INDEX_NONE) LampMID = Mesh->CreateDynamicMaterialInstance(LampSlot);

	Fuel = FMath::Clamp(StartFuel, 0.f, MaxFuel);
	ScheduleNextBreakdown();

	if (HumLoopSound)
	{
		HumAudio->SetSound(HumLoopSound);
	}
	// Motor uğultusu jeneratörün yakınında duyulur (her yerden değil): susması da bir işaret
	HumAudio->bOverrideAttenuation = true;
	HumAudio->AttenuationOverrides.bAttenuate = true;
	HumAudio->AttenuationOverrides.bSpatialize = true;
	HumAudio->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	HumAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
	HumAudio->AttenuationOverrides.FalloffDistance = HumHearDistance;

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RegisterGenerator(this);
	}
	NotifyPowerChanged(false);
}

void ANLGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLamp();

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
	// Elle verilmiş benzin ağzı (ör. jeneratör binasının duvarındaki depo kapağı)
	if (!FuelInletOffset.IsNearlyZero()) return GetActorTransform().TransformPosition(FuelInletOffset);

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

// Arıza/duruş sesleri adanın her yerinden, jeneratör binası yönünden duyulur
static void NLGenFarEvent(ANLGenerator* Gen, USoundBase* Sound)
{
	if (UNLPowerSubsystem* Power = Gen->GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->PlayFarEvent(Sound, Gen->GetActorLocation());
	else UGameplayStatics::PlaySoundAtLocation(Gen, Sound, Gen->GetActorLocation());
}
#define FarEvent(S) NLGenFarEvent(this, S)

void ANLGenerator::BreakDown()
{
	if (bBroken) return;

	bBroken = true;
	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	UE_LOG(LogTemp, Log, TEXT("NLGenerator: broke down (game time %.0f s)"), GetWorld()->GetTimeSeconds());
	FarEvent(BreakdownSound);
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
			if (bPlayTransitionSounds) FarEvent(EngineStartSound);
			HumAudio->Play();
		}
	}
	else if (HumAudio->IsPlaying())
	{
		HumAudio->Stop();
		if (bPlayTransitionSounds) FarEvent(EngineStopSound);
	}

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RefreshPower();
	}
}

void ANLGenerator::UpdateLamp()
{
	if (!LampMID && !LampLight) return;
	const float T = GetWorld()->GetTimeSeconds();
	FLinearColor Color(0.15f, 1.f, 0.25f);
	float Intensity = 2.2f;
	if (bBroken)
	{
		// Arıza: kırmızı, hızlı yanıp söner
		Color = FLinearColor(1.f, 0.06f, 0.03f);
		Intensity = FMath::Frac(T * 1.6f) < 0.5f ? 4.f : 0.05f;
	}
	else if (Fuel <= 0.f)
	{
		// Yakıt bitti: turuncu, yavaş nabız
		Color = FLinearColor(1.f, 0.45f, 0.05f);
		Intensity = 0.4f + 1.2f * (0.5f + 0.5f * FMath::Sin(T * 2.2f));
	}
	if (LampMID)
	{
		LampMID->SetVectorParameterValue(TEXT("Color"), Color);
		LampMID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
	}
	if (LampLight)
	{
		LampLight->SetLightColor(Color);
		LampLight->SetIntensity(Intensity * 0.35f);
	}
}
