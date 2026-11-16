#include "NLPowerSubsystem.h"
#include "NLGenerator.h"
#include "NLFuseBox.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

void UNLPowerSubsystem::RegisterGenerator(ANLGenerator* InGenerator)
{
	Generator = InGenerator;
	RefreshPower();
}

void UNLPowerSubsystem::RegisterFuseBox(ANLFuseBox* InFuseBox)
{
	FuseBox = InFuseBox;
	RefreshPower();
}

void UNLPowerSubsystem::RefreshPower()
{
	const bool bGeneratorOk = Generator.IsValid() && Generator->IsProducingPower();
	// Sahnede fuse yoksa jeneratör doğrudan besler
	const bool bFuseOk = !FuseBox.IsValid() || FuseBox->IsPowerOk();
	const bool bNewPower = bGeneratorOk && bFuseOk;

	if (bNewPower != bHasPower)
	{
		bHasPower = bNewPower;
		UE_LOG(LogTemp, Log, TEXT("NLPower: power %s"), bHasPower ? TEXT("ON") : TEXT("OFF"));
		// Oyun başındaki ilk durum sessiz; sonrasında binanın içinden röle + lamba uğultusu (her yerden duyulur, yönsüz)
		UWorld* W = GetWorld();
		if (W && W->HasBegunPlay() && W->GetTimeSeconds() > 1.f)
		{
			if (!PowerDownSound) PowerDownSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/NL/power_down.power_down"));
			if (!PowerUpSound) PowerUpSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/NL/power_up.power_up"));
			UGameplayStatics::PlaySound2D(W, bHasPower ? PowerUpSound.Get() : PowerDownSound.Get(), 0.8f);
			UE_LOG(LogTemp, Log, TEXT("NLPower: sound cue %s (building)"), bHasPower ? TEXT("power_up") : TEXT("power_down"));
		}
		OnPowerChanged.Broadcast(bHasPower);
	}
}

void UNLPowerSubsystem::PlayFarEvent(USoundBase* Sound, const FVector& Location, float Volume)
{
	if (!Sound || !GetWorld()) return;
	if (!FarAttenuation)
	{
		FarAttenuation = NewObject<USoundAttenuation>(this);
		FSoundAttenuationSettings& A = FarAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(800.f, 0.f, 0.f);
		A.FalloffDistance = 9000.f;
		A.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
	}
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Location, FRotator::ZeroRotator, Volume, 1.f, 0.f, FarAttenuation);
	const APawn* P = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	UE_LOG(LogTemp, Log, TEXT("NLPower: sound cue %s, %.0f m from the player"), *Sound->GetName(), P ? FVector::Dist(P->GetActorLocation(), Location) / 100.f : -1.f);
}

void UNLPowerSubsystem::MarkFailureActive()
{
	LastFailureTime = GetWorld()->GetTimeSeconds();
}

bool UNLPowerSubsystem::CanStartFailure(float& OutWait) const
{
	const float Since = GetWorld()->GetTimeSeconds() - LastFailureTime;
	OutWait = FMath::Max(0.f, MinFailureGap - Since);
	return OutWait <= 0.f;
}
