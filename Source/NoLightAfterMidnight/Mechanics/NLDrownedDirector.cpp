#include "NLDrownedDirector.h"
#include "NLDefaults.h"
#include "NLDrowned.h"
#include "NLSecurityDesk.h"
#include "NLSecurityCamera.h"
#include "NLGameMode.h"
#include "NLPlayerCharacter.h"
#include "NLFlashlightComponent.h"
#include "NLRunner.h"
#include "NLGenerator.h"
#include "NLFuseBox.h"
#include "NLPowerSubsystem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ANLDrownedDirector::ANLDrownedDirector()
{
	// İlk kaçırmada çalan uyarı (Content/Audio/NL)
	NL_DEFAULT_SOUND(MissWarningSound, "drowned_warning");
	NL_DEFAULT_SOUND(LeaveMapSound, "drowned_leave");
	NL_DEFAULT_SOUND(ReturnSound, "drowned_return");

	PrimaryActorTick.bCanEverTick = true;
	DrownedClass = ANLDrowned::StaticClass();
}

void ANLDrownedDirector::BeginPlay()
{
	Super::BeginPlay();

	if (PlacedDrowned)
	{
		Drowned = PlacedDrowned;
	}
	else if (DrownedClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Drowned = GetWorld()->SpawnActor<ANLDrowned>(DrownedClass, GetActorTransform(), Params);
	}

	if (Desk)
	{
		for (ANLSecurityCamera* Cam : Desk->GetCameras())
		{
			if (Cam) Cam->OnFlashed.AddDynamic(this, &ANLDrownedDirector::HandleCameraFlashed);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NLDrownedDirector: Desk atanmamış! Details panelinden masayı seçin."));
	}

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		GM->OnDayStarted.AddDynamic(this, &ANLDrownedDirector::HandleDayStarted);
		GM->OnDayEnded.AddDynamic(this, &ANLDrownedDirector::HandleDayEnded);
		GM->OnGameOver.AddDynamic(this, &ANLDrownedDirector::HandleGameOver);
	}
}

void ANLDrownedDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (State == ENLDrownedState::Inactive || State == ENLDrownedState::Attacking) return;

	// Kaos varken (koşucu / arıza) Drowned gelmez; gelmişse çekilir
	if (bHoldDuringChaos)
	{
		const TCHAR* Why = TEXT("");
		if (IsChaos(Why))
		{
			CalmTimer = CalmDelayAfterChaos;
			if (!bChaosLogged)
			{
				UE_LOG(LogTemp, Log, TEXT("NLDrowned: on hold (%s)"), Why);
				bChaosLogged = true;
			}
			if (State == ENLDrownedState::Arriving || State == ENLDrownedState::Watching)
			{
				UE_LOG(LogTemp, Log, TEXT("NLDrowned: withdraws from camera %d because of %s"), CurrentCameraIndex + 1, Why);
				if (Drowned) Drowned->RunOffAndVanish();
				SetState(ENLDrownedState::Waiting, RandomRelocateDelay());
				FlashlightLitTime = 0.f;
			}
		}
		else
		{
			CalmTimer = FMath::Max(0.f, CalmTimer - DeltaSeconds);
			if (bChaosLogged && CalmTimer <= 0.f)
			{
				UE_LOG(LogTemp, Log, TEXT("NLDrowned: calm again, may appear"));
				bChaosLogged = false;
			}
		}
		// Belirme zamanı geldi ama ortalık sakin değil: bekle
		if ((State == ENLDrownedState::Waiting || State == ENLDrownedState::Away) && CalmTimer > 0.f && StateTimer - DeltaSeconds <= 0.f)
		{
			StateTimer = FMath::Max(StateTimer, CalmTimer);
			return;
		}
	}

	// Kameranın önüne vardı: tepki süresi şimdi başlar
	if (State == ENLDrownedState::Arriving && Drowned && Drowned->HasArrived())
	{
		SetState(ENLDrownedState::Watching, GetReactionTime());
		UE_LOG(LogTemp, Log, TEXT("NLDrowned: arrived at camera %d"), CurrentCameraIndex + 1);
	}

	// Oyuncu yakınındaysa el fenerini doğrudan tutarak kovalayabilir (masadan ışık tutmakla aynı)
	if (State == ENLDrownedState::Watching || State == ENLDrownedState::Arriving)
	{
		FlashlightLitTime = IsLitByPlayerFlashlight() ? FlashlightLitTime + DeltaSeconds : 0.f;
		if (FlashlightLitTime >= FlashlightRepelTime)
		{
			FlashlightLitTime = 0.f;
			if (const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0)) { if (Drowned) Drowned->FaceTowards(P->GetActorLocation()); }
			UE_LOG(LogTemp, Log, TEXT("NLDrowned: repelled by the player's flashlight"));
			StartFlinch();
			return;
		}
	}

	StateTimer -= DeltaSeconds;
	if (StateTimer > 0.f) return;

	switch (State)
	{
	case ENLDrownedState::Waiting:   AppearAtRandomCamera();   break;
	case ENLDrownedState::Watching:  OnReactionTimeExpired();  break;
	case ENLDrownedState::Arriving:
		// Yol uzun sürdü: yerine ışınla
		if (ANLSecurityCamera* Cam = GetCurrentCamera()) { if (Drowned) Drowned->AppearAt(Cam->GetDrownedSpawnTransform()); }
		SetState(ENLDrownedState::Watching, GetReactionTime());
		break;
	case ENLDrownedState::Flinching: OnFlinchFinished();       break;
	case ENLDrownedState::Away:
		UGameplayStatics::PlaySound2D(this, ReturnSound);
		AppearAtRandomCamera();
		break;
	default: break;
	}
}

bool ANLDrownedDirector::IsChaos(const TCHAR*& OutWhy) const
{
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
	{
		if (It->IsChasing()) { OutWhy = TEXT("runner chase"); return true; }
	}
	if (const UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		const ANLGenerator* Gen = Power->GetGenerator();
		const ANLFuseBox* Fuse = Power->GetFuseBox();
		if (Gen && Gen->IsBroken()) { OutWhy = TEXT("generator broken"); return true; }
		if (Fuse && Fuse->IsTripped()) { OutWhy = TEXT("fuse tripped"); return true; }
		if ((Gen || Fuse) && !Power->HasPower()) { OutWhy = TEXT("no power"); return true; }
	}
	return false;
}

ANLSecurityCamera* ANLDrownedDirector::GetCurrentCamera() const
{
	if (!Desk) return nullptr;
	const auto& Cams = Desk->GetCameras();
	return Cams.IsValidIndex(CurrentCameraIndex) ? Cams[CurrentCameraIndex].Get() : nullptr;
}

void ANLDrownedDirector::SetState(ENLDrownedState NewState, float Duration)
{
	State = NewState;
	StateTimer = Duration;
}

void ANLDrownedDirector::AppearAtRandomCamera()
{
	if (!Desk || !Drowned || Desk->GetCameras().Num() == 0)
	{
		SetState(ENLDrownedState::Waiting, 5.f);
		return;
	}

	const int32 Num = Desk->GetCameras().Num();
	int32 NewIndex = FMath::RandRange(0, Num - 1);
	// "Drowned başka kameraya kaçar": aynı kamerada iki kez üst üste belirmesin
	if (Num > 1 && NewIndex == CurrentCameraIndex)
	{
		NewIndex = (NewIndex + FMath::RandRange(1, Num - 1)) % Num;
	}
	CurrentCameraIndex = NewIndex;

	if (ANLSecurityCamera* Cam = GetCurrentCamera())
	{
		Drowned->ApproachTo(Cam->GetDrownedSpawnTransform());
	}
	// Önce koşarak gelir; varınca Watching (yol yoksa ApproachTo hemen belirtir)
	SetState(ENLDrownedState::Arriving, MaxArriveTime);
	FlashlightLitTime = 0.f;
}

void ANLDrownedDirector::DebugAppear(int32 Index)
{
	if (!Desk || !Drowned || Desk->GetCameras().Num() == 0) return;
	if (Index >= 0 && Desk->GetCameras().IsValidIndex(Index))
	{
		CurrentCameraIndex = Index;
		if (ANLSecurityCamera* Cam = GetCurrentCamera()) Drowned->ApproachTo(Cam->GetDrownedSpawnTransform());
		SetState(ENLDrownedState::Arriving, MaxArriveTime);
		FlashlightLitTime = 0.f;
	}
	else AppearAtRandomCamera();
}

bool ANLDrownedDirector::IsLitByPlayerFlashlight() const
{
	const ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || !Drowned || !Drowned->IsShown() || Player->GetMode() != ENLPlayerMode::Walking) return false;
	const UNLFlashlightComponent* Light = Player->GetFlashlight();
	if (!Light || !Light->IsOn()) return false;

	const FVector From = Light->GetComponentLocation();
	const FVector Chest = Drowned->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	const FVector To = Chest - From;
	const float Dist = To.Size();
	if (Dist > FMath::Min(FlashlightRepelRange, Light->AttenuationRadius)) return false;
	// Işık konisinin içinde mi (kenarlar hariç)
	const float Cos = FVector::DotProduct(To / FMath::Max(Dist, 1.f), Light->GetForwardVector());
	if (Cos < FMath::Cos(FMath::DegreesToRadians(Light->OuterConeAngle * 0.85f))) return false;
	// Arada duvar/ağaç yok
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLDrownedLit), false, Player);
	Q.AddIgnoredActor(Drowned);
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, Chest, ECC_Visibility, Q);
}

void ANLDrownedDirector::HandleCameraFlashed(ANLSecurityCamera* Camera)
{
	if ((State != ENLDrownedState::Watching && State != ENLDrownedState::Arriving) || Camera != GetCurrentCamera()) return;
	StartFlinch();
}

void ANLDrownedDirector::StartFlinch()
{
	// Önce elini yüzüne kapatır...
	bFleePhase = false;
	const float Duration = Drowned ? Drowned->PlayCoverFace() : 1.f;
	SetState(ENLDrownedState::Flinching, Duration);
}

void ANLDrownedDirector::OnFlinchFinished()
{
	// ...sonra kaçar
	if (!bFleePhase)
	{
		bFleePhase = true;
		const float Duration = Drowned ? Drowned->PlayFlee() : 0.5f;
		SetState(ENLDrownedState::Flinching, Duration);
		return;
	}

	bFleePhase = false;
	// Geldiği hattan geri koşup kaybolur
	if (Drowned) Drowned->RunOffAndVanish();
	++EscapeCount;
	MissCount = 0;

	if (EscapeCount >= EscapesBeforeLeaving)
	{
		// 3 kez tekrarlandı: haritadan ayrılır, 2 dk sonra döner
		EscapeCount = 0;
		CurrentCameraIndex = INDEX_NONE;
		UGameplayStatics::PlaySound2D(this, LeaveMapSound);
		SetState(ENLDrownedState::Away, AwayDuration);
	}
	else
	{
		SetState(ENLDrownedState::Waiting, RandomRelocateDelay());
	}
}

void ANLDrownedDirector::OnReactionTimeExpired()
{
	// Oyuncu masada değilse (dışarıda geziyor, tamir ediyor...) kaçırma sayılmaz: Drowned hattından geri koşup başka kameraya geçer.
	// Dışarıdayken onu sadece el feneriyle kovalayabilirsin; jumpscare yalnızca masadayken kaçırılırsa gelir.
	const ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || Player->GetMode() != ENLPlayerMode::Seated)
	{
		UE_LOG(LogTemp, Log, TEXT("NLDrowned: light missed while the player is away from the desk -> not counted (%d / %d)"), MissCount, MissesBeforeAttack);
		if (Drowned) Drowned->RunOffAndVanish();
		SetState(ENLDrownedState::Waiting, RandomRelocateDelay() * 0.5f);
		return;
	}

	++MissCount;
	UE_LOG(LogTemp, Log, TEXT("NLDrowned: light missed (%d / %d) at %.0f s"), MissCount, MissesBeforeAttack, GetWorld()->GetTimeSeconds());
	if (MissCount >= MissesBeforeAttack)
	{
		StartAttack();
		return;
	}

	// Henüz saldırmaz: uyarı verir ve başka kameraya geçer
	UGameplayStatics::PlaySound2D(this, MissWarningSound);
	if (Drowned) Drowned->RunOffAndVanish();
	SetState(ENLDrownedState::Waiting, RandomRelocateDelay() * 0.5f);
}

void ANLDrownedDirector::StartAttack()
{
	SetState(ENLDrownedState::Attacking, 0.f);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (ANLPlayerCharacter* Player = PC ? Cast<ANLPlayerCharacter>(PC->GetPawn()) : nullptr)
	{
		Player->Die();
	}

	const float JumpscareTime = Drowned ? Drowned->PerformJumpscare(PC) : 1.f;

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		GM->TriggerGameOver(ENLGameOverReason::Drowned, JumpscareTime + ExtraDelayAfterJumpscare);
	}
}

float ANLDrownedDirector::GetReactionTime() const
{
	return FMath::Max(MinReactionTime, ReactionTime - ReactionTimeReductionPerDay * (CurrentDay - 1));
}

float ANLDrownedDirector::RandomRelocateDelay() const
{
	return FMath::FRandRange(MinRelocateDelay, FMath::Max(MinRelocateDelay, MaxRelocateDelay));
}

void ANLDrownedDirector::HandleDayStarted(int32 Day)
{
	CurrentDay = Day;
	EscapeCount = 0;
	MissCount = 0;
	CurrentCameraIndex = INDEX_NONE;
	bFleePhase = false;
	if (Drowned) Drowned->Vanish();
	const float Delay = Day <= 1 ? FirstNightAppearanceDelay : FirstAppearanceDelay;
	UE_LOG(LogTemp, Log, TEXT("NLDrowned: night %d, first appearance in %.0f s"), Day, Delay);
	SetState(ENLDrownedState::Waiting, Delay);
}

void ANLDrownedDirector::HandleDayEnded(int32 Day)
{
	if (Drowned) Drowned->Vanish();
	SetState(ENLDrownedState::Inactive, 0.f);
}

void ANLDrownedDirector::HandleGameOver(ENLGameOverReason Reason)
{
	// Runner öldürdüyse Drowned ortadan kalkar; Drowned'ın kendi saldırısına dokunma
	if (State != ENLDrownedState::Attacking)
	{
		if (Drowned) Drowned->Vanish();
		SetState(ENLDrownedState::Inactive, 0.f);
	}
}
