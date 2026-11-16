#include "NLRunnerAmbushZone.h"
#include "NLRunner.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

ANLRunnerAmbushZone::ANLRunnerAmbushZone()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANLRunnerAmbushZone::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It) { Runner = *It; break; }
}

float ANLRunnerAmbushZone::GetChanceForDay(int32 Day) const
{
	return FMath::Min(ChanceDay1 + ChanceIncreasePerDay * FMath::Max(Day - 1, 0), MaxChance);
}

void ANLRunnerAmbushZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Saniyede 4 kez kontrol yeter
	CheckTimer -= DeltaSeconds;
	if (CheckTimer > 0.f) return;
	CheckTimer = 0.25f;

	ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	if (!Player || !GM) return;

	const bool bInside = FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) < Radius;
	const bool bEntered = bInside && !bPlayerInside;
	bPlayerInside = bInside;
	if (!bEntered) return;

	// Yeni gün: sayaç sıfırlanır
	const int32 Day = GM->GetCurrentDay();
	if (Day != CountedDay)
	{
		CountedDay = Day;
		AmbushesToday = 0;
	}

	ANLRunner* R = Runner.Get();
	if (!R || !GM->IsDayActive() || R->IsChasing() || Player->GetMode() != ENLPlayerMode::Walking) return;
	if (AmbushesToday >= MaxPerDay && !bDebugForce) return;

	const float Chance = GetChanceForDay(Day);
	const float Roll = FMath::FRand();
	const bool bAmbush = bDebugForce || Roll < Chance;
	UE_LOG(LogTemp, Log, TEXT("NLAmbush: player entered %s (day %d, chance %.0f%%, roll %.2f) -> %s"),
		*GetActorNameOrLabel(), Day, Chance * 100.f, Roll, bAmbush ? TEXT("AMBUSH") : TEXT("nothing"));
	if (!bAmbush) return;

	bDebugForce = false;
	++AmbushesToday;
	R->PrepareAmbush(Player, ScreamToChaseDelay, SpawnMinDistance, SpawnMaxDistance);
}
