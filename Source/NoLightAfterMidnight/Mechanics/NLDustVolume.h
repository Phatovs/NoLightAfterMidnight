// NoLightAfterMidnight - Havada süzülen toz
// Kutunun içine minik toz zerreleri serpilir; her biri kendi yolunda yavaşça salınır (materyalde, işlemci yükü yok).
// Zerreler ışıktan etkilenir: karanlıkta görünmez, fener ya da lamba vurunca ışık huzmesinde parlar.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLDustVolume.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLDustVolume : public AActor
{
	GENERATED_BODY()

public:
	ANLDustVolume();
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NL")
	void RebuildNow() { Rebuild(); }

	// Yarı boyutlar (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Dust") FVector Extent = FVector(200.f, 200.f, 120.f);
	// Metreküp başına zerre
	UPROPERTY(EditAnywhere, Category = "NL|Dust") float Density = 45.f;
	UPROPERTY(EditAnywhere, Category = "NL|Dust") float MoteScale = 1.3f;
	UPROPERTY(EditAnywhere, Category = "NL|Dust") int32 Seed = 1;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Motes;

private:
	void Rebuild();
};
