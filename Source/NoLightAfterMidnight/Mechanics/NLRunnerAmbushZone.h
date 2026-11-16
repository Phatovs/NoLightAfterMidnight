// NoLightAfterMidnight - Runner pusu alanı (rastgele olay)
// Oyuncu alana girince (ör. benzin kulübesinin çevresi) düşük bir ihtimalle Runner uzakta çığlık atar, birkaç saniye sonra kovalar.
// Oyuncu yakındaki saklanma kapısına (ANLHideDoor) kaçıp kapıyı tutarak kurtulabilir.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLRunnerAmbushZone.generated.h"

class ANLRunner;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLRunnerAmbushZone : public AActor
{
	GENERATED_BODY()

public:
	ANLRunnerAmbushZone();
	virtual void Tick(float DeltaSeconds) override;

	// Alanın yarıçapı (cm): oyuncu bu mesafeye girince zar atılır
	UPROPERTY(EditAnywhere, Category = "NL|Ambush") float Radius = 1400.f;
	// 1. gün her girişte pusu ihtimali; sonraki günlerde gün başına artar, üst sınırı var
	UPROPERTY(EditAnywhere, Category = "NL|Ambush", meta = (ClampMin = "0", ClampMax = "1")) float ChanceDay1 = 0.2f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambush", meta = (ClampMin = "0", ClampMax = "1")) float ChanceIncreasePerDay = 0.07f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambush", meta = (ClampMin = "0", ClampMax = "1")) float MaxChance = 0.55f;
	// Gün başına en fazla kaç pusu
	UPROPERTY(EditAnywhere, Category = "NL|Ambush", meta = (ClampMin = "1")) int32 MaxPerDay = 1;
	// Çığlıktan sonra Runner'ın koşmaya başlaması (sn): oyuncunun kulübeye kaçma süresi
	UPROPERTY(EditAnywhere, Category = "NL|Ambush") float ScreamToChaseDelay = 3.f;
	// Runner'ın oyuncudan ne kadar uzakta belireceği (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Ambush") float SpawnMinDistance = 1600.f;
	UPROPERTY(EditAnywhere, Category = "NL|Ambush") float SpawnMaxDistance = 2800.f;

	// Test: bir sonraki girişte kesin pusu
	void SetDebugForce(bool bForce) { bDebugForce = bForce; }
	float GetChanceForDay(int32 Day) const;

protected:
	virtual void BeginPlay() override;

private:
	bool bPlayerInside = false;
	bool bDebugForce = false;
	int32 CountedDay = -1;
	int32 AmbushesToday = 0;
	float CheckTimer = 0.f;
	TWeakObjectPtr<ANLRunner> Runner;
};
