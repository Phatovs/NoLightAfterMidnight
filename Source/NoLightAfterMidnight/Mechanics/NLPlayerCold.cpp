// NoLightAfterMidnight - Oyuncu: soğuk / donma
// Dışarıda (başının üstünde çatı, çevresinde duvar yok) geçen süre birikir:
//   ColdGraceTime (60 sn)            : üşür; ekran kenarlarından buz tutmaya başlar, renkler solar, titreme
//   + FreezeDelay (15 sn) = 75 sn     : donmaya başlar; hızı FreezeRampTime içinde FreezeMinSpeed'e düşer, Runner uzaktan çığlık atıp gelir
// İçeride (çatı altı) birikmez; çalışan bir sobanın (ANLHeater) yanında hızla ısınır. Bir binaya girince Runner vazgeçer.
// Soğuk, oyuncu o gece ilk kez bir binaya girince başlar (1. gece rıhtımdan fenere yürüyüşte işlemez).
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "NLHeater.h"
#include "NLLighthouse.h"
#include "NLRunner.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

float ANLPlayerCharacter::GetColdAlpha() const
{
	if (ColdExposure <= ColdGraceTime) return 0.f;
	const float Freeze = ColdGraceTime + FreezeDelay;
	if (ColdExposure < Freeze) return 0.6f * (ColdExposure - ColdGraceTime) / FMath::Max(FreezeDelay, 0.1f);
	return FMath::Min(1.f, 0.6f + 0.4f * (ColdExposure - Freeze) / FMath::Max(FreezeRampTime, 0.1f));
}

float ANLPlayerCharacter::GetColdSpeedMultiplier() const
{
	const float Over = ColdExposure - (ColdGraceTime + FreezeDelay);
	if (Over <= 0.f) return 1.f;
	const float A = FMath::Clamp(Over / FMath::Max(FreezeRampTime, 0.1f), 0.f, 1.f);
	return FMath::Lerp(0.8f, FreezeMinSpeed, FMath::SmoothStep(0.f, 1.f, A));   // donunca hemen biraz, sonra giderek yavaşlar
}

void ANLPlayerCharacter::UpdateCold(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const ANLGameMode* GM = World ? World->GetAuthGameMode<ANLGameMode>() : nullptr;
	bColdRunning = bColdEnabled && GM && GM->IsDayActive() && !bCinematic && Mode != ENLPlayerMode::Dead;

	// Barınak ve soba (4 kez/sn): başın üstünde çatı + çevrede en az 3 yönde duvar (ağaç altı sayılmaz)
	ColdCheckTimer -= DeltaSeconds;
	if (ColdCheckTimer <= 0.f && World)
	{
		ColdCheckTimer = 0.25f;
		const FVector Head = GetActorLocation() + FVector(0.f, 0.f, 70.f);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NLColdShelter), false, this);
		FHitResult Hit;
		bool bRoof = World->LineTraceSingleByChannel(Hit, Head, Head + FVector(0.f, 0.f, 700.f), ECC_Visibility, Q);
		int32 Walls = 0;
		if (bRoof)
		{
			const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 30.f);
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector Dir = FRotator(0.f, 45.f + 90.f * i, 0.f).Vector();
				if (World->LineTraceSingleByChannel(Hit, Chest, Chest + Dir * 900.f, ECC_Visibility, Q)) ++Walls;
			}
		}
		const bool bNow = bRoof && Walls >= 3;
		if (bNow != bSheltered) UE_LOG(LogTemp, Log, TEXT("NLCold: player %s (exposure %.1f s)"), bNow ? TEXT("sheltered") : TEXT("outside"), ColdExposure);
		bSheltered = bNow;
		const bool bHeat = ANLHeater::IsWarmAt(World, GetActorLocation());
		if (bHeat != bNearHeater) UE_LOG(LogTemp, Log, TEXT("NLCold: %s heater (exposure %.1f s)"), bHeat ? TEXT("near") : TEXT("left"), ColdExposure);
		bNearHeater = bHeat;
		if (bSheltered && !bColdArmed && bColdRunning)
		{
			bColdArmed = true;
			UE_LOG(LogTemp, Log, TEXT("NLCold: armed (player reached shelter)"));
		}
	}

	const bool bActive = bColdArmed && bColdRunning;
	if (bActive)
	{
		if (bNearHeater) ColdExposure -= DeltaSeconds * HeaterWarmRate;
		else if (bSheltered) ColdExposure -= DeltaSeconds * ShelterWarmRate;
		else ColdExposure += DeltaSeconds;
		ColdExposure = FMath::Max(0.f, ColdExposure);
	}
	else if (!bColdEnabled || Mode == ENLPlayerMode::Dead)
	{
		ColdExposure = 0.f;
	}

	// Donma başladı: Runner gelir (gelmezse / vazgeçerse ara ara tekrar)
	const bool bFreezing = bActive && IsFreezing();
	const bool bJustFroze = bFreezing && !bWasFreezing;
	if (bFreezing && !bWasFreezing)
	{
		UE_LOG(LogTemp, Log, TEXT("NLCold: freezing (exposure %.1f s)"), ColdExposure);
		ColdRunnerTimer = 0.f;
	}
	if (!bFreezing && bWasFreezing) UE_LOG(LogTemp, Log, TEXT("NLCold: no longer freezing (exposure %.1f s)"), ColdExposure);
	bWasFreezing = bFreezing;
	if (bFreezing && !bSheltered)
	{
		ColdRunnerTimer -= DeltaSeconds;
		if (ColdRunnerTimer <= 0.f)
		{
			ColdRunnerTimer = ColdRunnerRetry;
			if (ANLGameMode* G = World->GetAuthGameMode<ANLGameMode>()) G->RequestColdRunner(this);
		}
	}

	// Ses: buz büyürken çıtırtı; üşümeye ve donmaya başlarken, donarken de ara ara buz çatlaması
	const bool bCold = bActive && IsGettingCold();
	const bool bGrowing = bCold && !bSheltered && !bNearHeater;
	if (bCold && !bWasCold && FrostCrackSound) UGameplayStatics::PlaySound2D(this, FrostCrackSound, FrostVolume * 0.6f, FMath::FRandRange(0.95f, 1.08f));
	if (bFreezing && FrostCrackSound && FrostCrackTimer <= 0.f && bGrowing)
	{
		UGameplayStatics::PlaySound2D(this, FrostCrackSound, FrostVolume * (bJustFroze ? 1.1f : 0.75f), FMath::FRandRange(0.88f, 1.1f));
		FrostCrackTimer = FMath::FRandRange(5.f, 11.f);
	}
	FrostCrackTimer = bFreezing ? FrostCrackTimer - DeltaSeconds : 0.f;
	bWasCold = bCold;
	const float WantVol = bGrowing ? FrostVolume * (0.35f + 0.65f * GetColdAlpha()) : 0.f;
	FrostVol = FMath::FInterpTo(FrostVol, WantVol, DeltaSeconds, WantVol > FrostVol ? 1.5f : 2.5f);
	if (FrostCrackleLoop)
	{
		if (FrostVol > 0.01f)
		{
			if (!FrostAudio->IsPlaying()) FrostAudio->Play(FMath::FRandRange(0.f, 5.f));
			FrostAudio->SetVolumeMultiplier(FrostVol);
		}
		else if (FrostAudio->IsPlaying())
		{
			FrostAudio->Stop();
		}
	}

	// Ekran efekti ve titreme
	ColdFx = FMath::FInterpTo(ColdFx, bActive ? GetColdAlpha() : 0.f, DeltaSeconds, 1.5f);
	if (ColdFx > 0.01f && (Mode == ENLPlayerMode::Walking || Mode == ENLPlayerMode::HoldingDoor))
	{
		// Titreme: bakışa küçük, hızlı ve düzensiz salınım (toplamı sıfır: bakış kaymaz)
		const float Prev = ShiverPhase;
		ShiverPhase += DeltaSeconds;
		auto Wave = [](float T) { return FMath::Sin(T * 31.f) * 0.6f + FMath::Sin(T * 53.f + 1.7f) * 0.4f; };
		const float Amp = 0.06f * ColdFx * ColdFx;
		AddControllerPitchInput(-(Wave(ShiverPhase) - Wave(Prev)) * Amp);
		AddControllerYawInput((Wave(ShiverPhase * 0.83f + 4.f) - Wave(Prev * 0.83f + 4.f)) * Amp * 0.6f);
	}
}

void ANLPlayerCharacter::NLDebugSetCold(float Seconds)
{
	bColdArmed = true;
	ColdExposure = FMath::Max(0.f, Seconds);
	UE_LOG(LogTemp, Log, TEXT("NLCold: exposure set to %.1f s"), ColdExposure);
}

void ANLPlayerCharacter::NLDebugCold(float TimeScale)
{
	// Dışarıda bekle -> üşü -> don (Runner gelir) -> eve kaç, sobada ısın (Runner vazgeçer)
	UWorld* World = GetWorld();
	ANLLighthouse* LH = nullptr;
	for (TActorIterator<ANLLighthouse> It(World); It; ++It) { LH = *It; break; }
	if (!LH) { UE_LOG(LogTemp, Warning, TEXT("NLDebugCold: no lighthouse")); return; }
	const FTransform X = LH->GetActorTransform();
	const float FloorZ = 40.f * LH->TowerHeight / 1000.f;
	const float S = FMath::Clamp(TimeScale, 1.f, 20.f);
	if (S > 1.f) World->GetWorldSettings()->SetTimeDilation(S);

	auto Place = [this, X, FloorZ](const FVector& Local, const FVector& LookLocal)
	{
		const FVector P = X.TransformPosition(Local + FVector(0.f, 0.f, FloorZ + 100.f));
		const FVector L = X.TransformPosition(LookLocal + FVector(0.f, 0.f, FloorZ));
		SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* C = GetController()) C->SetControlRotation((L - (P + FVector(0.f, 0.f, 60.f))).Rotation());
	};
	FTimerManager& TM = GetWorldTimerManager();
	// Zamanlayıcılar oyun zamanıyla işler (hızlandırma süreleri değiştirmez)
	auto At = [this, &TM](float Time, TFunction<void()> Fn)
	{
		FTimerHandle H;
		TM.SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn]() { Fn(); }), Time, false);
	};
	auto Shot = [this](const FString& Name)
	{
		int32 Chasing = 0;
		FVector RunnerLoc = FVector::ZeroVector;
		for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) if (It->IsChasing()) { ++Chasing; RunnerLoc = It->GetActorLocation(); }
		UE_LOG(LogTemp, Log, TEXT("NLDebugCold: %s exposure=%.1f alpha=%.2f heat=%.2f sheltered=%d heater=%d freezing=%d speedMul=%.2f maxSpeed=%.0f runner=%d dist=%.0f"),
			*Name, ColdExposure, GetColdAlpha(), GetBodyHeat(), bSheltered ? 1 : 0, bNearHeater ? 1 : 0, IsFreezing() ? 1 : 0, GetColdSpeedMultiplier(),
			GetCharacterMovement()->MaxWalkSpeed, Chasing, Chasing ? FVector::Dist(RunnerLoc, GetActorLocation()) : -1.f);
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("shot showui"));
	};

	// İçeride soba (gerçek zaman, ölçek öncesi)
	Place(FVector(600.f, -40.f, 0.f), FVector(292.f, -95.f, 40.f));
	At(2.f, [Shot]() { Shot(TEXT("01 heater in house")); });
	// Dışarı çık, soğuk birikimini 50 sn'ye al (bekleme kısalsın)
	At(3.f, [this, Place]() { Place(FVector(640.f, -1100.f, 0.f), FVector(640.f, -2200.f, 100.f)); bColdArmed = true; ColdExposure = 50.f; });
	At(6.f, [Shot]() { Shot(TEXT("02 outside, fine")); });
	At(14.f, [Shot]() { Shot(TEXT("03 getting cold (frost begins)")); });
	At(24.f, [Shot]() { Shot(TEXT("04 about to freeze")); });
	At(29.f, [Shot]() { Shot(TEXT("05 freezing, runner screamed")); });
	At(36.f, [Shot]() { Shot(TEXT("06 frozen, slow, runner coming")); });
	At(37.f, [this, Place]() { Place(FVector(640.f, -1100.f, 0.f), FVector(640.f, 0.f, 100.f)); });
	At(37.5f, [Shot]() { Shot(TEXT("07 looking back to the house")); });
	// Eve girip sobanın önüne
	At(38.f, [this, Place]() { Place(FVector(420.f, -90.f, 0.f), FVector(292.f, -95.f, 40.f)); });
	At(45.f, [Shot]() { Shot(TEXT("08 inside at the heater")); });
	At(50.f, [Shot]() { Shot(TEXT("09 warming up")); });
	At(56.f, [Shot]() { Shot(TEXT("10 warm again")); });
	At(58.f, [this]() { if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->ConsoleCommand(TEXT("quit")); });
}
