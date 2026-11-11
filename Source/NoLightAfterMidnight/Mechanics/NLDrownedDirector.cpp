#include "NLDrownedDirector.h"
#include "NLDefaults.h"
#include "NLDrowned.h"
#include "NLSecurityDesk.h"
#include "NLSecurityCamera.h"
#include "NLGameMode.h"
#include "NLPlayerCharacter.h"
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

	StateTimer -= DeltaSeconds;
	if (StateTimer > 0.f) return;

	switch (State)
	{
	case ENLDrownedState::Waiting:   AppearAtRandomCamera();   break;
	case ENLDrownedState::Watching:  OnReactionTimeExpired();  break;
	case ENLDrownedState::Flinching: OnFlinchFinished();       break;
	case ENLDrownedState::Away:
		UGameplayStatics::PlaySound2D(this, ReturnSound);
		AppearAtRandomCamera();
		break;
	default: break;
	}
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
		Drowned->AppearAt(Cam->GetDrownedSpawnTransform());
	}
	SetState(ENLDrownedState::Watching, GetReactionTime());
}

void ANLDrownedDirector::HandleCameraFlashed(ANLSecurityCamera* Camera)
{
	if (State != ENLDrownedState::Watching || Camera != GetCurrentCamera()) return;

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
	if (Drowned) Drowned->Vanish();
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
	++MissCount;
	UE_LOG(LogTemp, Log, TEXT("NLDrowned: light missed (%d / %d) at %.0f s"), MissCount, MissesBeforeAttack, GetWorld()->GetTimeSeconds());
	if (MissCount >= MissesBeforeAttack)
	{
		StartAttack();
		return;
	}

	// Henüz saldırmaz: uyarı verir ve başka kameraya geçer
	UGameplayStatics::PlaySound2D(this, MissWarningSound);
	if (Drowned) Drowned->Vanish();
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
	SetState(ENLDrownedState::Waiting, FirstAppearanceDelay);
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
