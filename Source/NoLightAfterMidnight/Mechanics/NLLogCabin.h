// NoLightAfterMidnight - Modüler kütük kulübe
// Parametrelerden kendini kurar (editörde anında görünür): köşelerde geçmeli kütük duvarlar, kütükten alınlıklar,
// kiremit sıralı beşik çatı, mahya ve aşık kütükleri, kapı/pencere boşlukları (kasa, cam, kayıt, açık kepenk),
// kapı yanında asılı fener (ışıklı), soba borusu, döşeme, kapı önü basamakları ve fıçı/sandık.
// "Yıkık" seçeneği: duvarlar yer yer yıkılmış, çatının bir kısmı yok, yerde devrik kütükler.
// Her parça türü tek instanced bileşen: kulübe başına ~10 çizim çağrısı.
// Yerel eksenler: X = ön duvar boyunca, -Y = ön (kapı tarafı), Z = yukarı; orijin = döşeme ortası, döşeme üstü.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLLogCabin.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ENLCabinWall : uint8
{
	Front,  // -Y
	Right,  // +X
	Back,   // +Y
	Left    // -X
};

USTRUCT(BlueprintType)
struct FNLCabinOpening
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Opening") ENLCabinWall Wall = ENLCabinWall::Front;
	// Duvarın ortasından kayma (duvar yönünde, cm)
	UPROPERTY(EditAnywhere, Category = "Opening") float Offset = 0.f;
	UPROPERTY(EditAnywhere, Category = "Opening") float Width = 110.f;
	UPROPERTY(EditAnywhere, Category = "Opening") float Height = 210.f;
	// Döşemeden alt kenara yükseklik (kapı için 0)
	UPROPERTY(EditAnywhere, Category = "Opening") float Sill = 0.f;
	UPROPERTY(EditAnywhere, Category = "Opening") bool bWindow = false;
	// Pencere: içeride ışık yanıyor mu (sıcak sarı cam) / kepenkler
	UPROPERTY(EditAnywhere, Category = "Opening") bool bLit = true;
	UPROPERTY(EditAnywhere, Category = "Opening") bool bShutters = true;
	// Kapı: tahta kapı kanadı (oyun kapısı NLHideDoor kullanılıyorsa kapat)
	UPROPERTY(EditAnywhere, Category = "Opening") bool bDoorPanel = true;
	// Kapı: yanına fener asılsın (-1 sol, 0 yok, 1 sağ)
	UPROPERTY(EditAnywhere, Category = "Opening") int32 LanternSide = 1;
	// Kasa/pervaz tahtaları
	UPROPERTY(EditAnywhere, Category = "Opening") bool bFrame = true;
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLLogCabin : public AActor
{
	GENERATED_BODY()

public:
	ANLLogCabin();
	virtual void OnConstruction(const FTransform& Transform) override;

	// Parametre değişince yeniden kur (editörde Details > NL > Rebuild Now, Python: rebuild_now())
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NL")
	void RebuildNow() { Rebuild(); }

	// --- Gövde ---
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") float Width = 600.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") float Depth = 450.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") float WallHeight = 260.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") float LogDiameter = 30.f;
	// Köşelerde kütüklerin dışarı taşması
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") float CornerOverhang = 24.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") bool bFloor = true;
	// Eğimli zemin: duvar kütükleri döşemenin bu kadar altına kadar iner (alttaki boşluk görünmesin)
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") float FoundationDepth = 0.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") TArray<FNLCabinOpening> Openings;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin") int32 Seed = 7;

	// --- Çatı ---
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Roof") float RoofPitch = 38.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Roof") float EaveOverhang = 50.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Roof") float GableOverhang = 45.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Roof") bool bChimney = true;
	// Soba borusu: X (mahya boyunca), arka yamaçta mahyadan uzaklık oranı (0..1)
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Roof") FVector2D ChimneyPos = FVector2D(-0.25f, 0.45f);

	// --- Süs ---
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Dressing") bool bPorchProps = true;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Dressing") float LanternIntensity = 8.f;    // kandela
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Dressing") float LanternRadius = 750.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Dressing") bool bInteriorLamp = true;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Dressing") float InteriorIntensity = 4.f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Dressing") FLinearColor LampColor = FLinearColor(1.f, 0.62f, 0.3f);

	// --- Yıkık ---
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Ruin") bool bRuined = false;
	// Duvar başına yükseklik (sıra: ön, sağ, arka, sol; 0 = tam)
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Ruin") TArray<float> RuinWallHeights = { 0.f, 110.f, 0.f, 0.f };
	// Çatının sol (-X) taraftan ne kadarı duruyor (0..1)
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Ruin") float RoofCoverage = 0.55f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Ruin") float MissingLogChance = 0.18f;
	UPROPERTY(EditAnywhere, Category = "NL|Cabin|Ruin") int32 FallenLogs = 4;

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Logs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Boards;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Shingles;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Deck;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> PaneLit;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> PaneDark;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Lanterns;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Pipes;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Barrels;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Crates;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> DoorLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> InteriorLight;

private:
	void Rebuild();
	void BuildWalls(float& OutWallTop);
	void BuildRoof(float WallTop);
	void BuildOpenings(float WallTop);
	void BuildFloorAndProps();
	void BuildRuinDebris();

	// Duvar bilgisi
	void WallFrame(ENLCabinWall W, FVector& Origin, FVector& Dir, FVector& Out, float& HalfLen) const;
	float WallLimit(ENLCabinWall W) const;
	void AddLog(const FVector& Center, const FVector& Dir, float Length, float Diameter);
	void AddBoard(const FVector& Center, const FVector& XDir, const FVector& ZHint, float Length, float WidthY, float Thick);

	FRandomStream Rng;
};
