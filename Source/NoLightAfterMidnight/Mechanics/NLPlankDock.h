// NoLightAfterMidnight - Modüler tahta iskele
// Döşeme panelleri (hafif eğri/aralıklı), kenar kirişleri, suya inen kazıklar, bağlama babaları + halat,
// uçta fenerli direkler (ışıklı). Yerel eksenler: X = iskelenin boyu (+X denize doğru), üst yüzey z=0.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLPlankDock.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLPlankDock : public AActor
{
	GENERATED_BODY()

public:
	ANLPlankDock();
	virtual void OnConstruction(const FTransform& Transform) override;

	// Parametre değişince yeniden kur (editörde Details > NL > Rebuild Now, Python: rebuild_now())
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NL")
	void RebuildNow() { Rebuild(); }

	UPROPERTY(EditAnywhere, Category = "NL|Dock") float Length = 1850.f;
	UPROPERTY(EditAnywhere, Category = "NL|Dock") float Width = 620.f;
	UPROPERTY(EditAnywhere, Category = "NL|Dock") float PostSpacing = 240.f;
	// Kazıkların suyun içine indiği derinlik (döşemenin altından)
	UPROPERTY(EditAnywhere, Category = "NL|Dock") float PostDepth = 380.f;
	// Her kaçıncı kazık döşemenin üstüne çıkıp baba olur
	UPROPERTY(EditAnywhere, Category = "NL|Dock") int32 BollardEvery = 3;
	// Döşemenin kara tarafında başlamadığı uzunluk (kulübe gibi şeyler için boş bırakılabilir)
	UPROPERTY(EditAnywhere, Category = "NL|Dock") bool bEndLanterns = true;
	UPROPERTY(EditAnywhere, Category = "NL|Dock") float LanternIntensity = 7.f;
	UPROPERTY(EditAnywhere, Category = "NL|Dock") float LanternRadius = 900.f;
	UPROPERTY(EditAnywhere, Category = "NL|Dock") FLinearColor LampColor = FLinearColor(1.f, 0.6f, 0.28f);
	UPROPERTY(EditAnywhere, Category = "NL|Dock") int32 Seed = 3;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Deck;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Logs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Ropes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> LanternPosts;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Lanterns;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> LightA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> LightB;

private:
	void Rebuild();
};
