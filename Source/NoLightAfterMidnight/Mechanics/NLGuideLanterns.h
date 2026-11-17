// NoLightAfterMidnight - Rıhtımdan deniz fenerine yol gösteren fenerli direkler
// Oyun başında NavMesh'ten rıhtımdan güvenlik masasına giden yolu bulur, yol boyunca aralıklarla fenerli direkler dikilir.
// Direkler toprak yolun (Content/Data/NLPathMask.png, arazideki patika maskesiyle aynı) kenarına konur, yolun ortasına değil.
// Taş kaideli ahşap direk, zincirle asılı fener; içinde mum alevi. Işık mum gibi yumuşak dalgalanır (hiç sönmez), fener rüzgârda hafifçe sallanır.
// Yolun başına (rıhtımdan çıkınca) "LIGHTHOUSE" yazan bir tabela konur. GameMode, rıhtım sinematiği (NLIntroDirector)
// ve güvenlik masası olan level'de kendisi oluşturur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLGuideLanterns.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

USTRUCT()
struct FNLGuideLamp
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glass;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Flame;
	float Seed = 0.f;
};

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLGuideLanterns : public AActor
{
	GENERATED_BODY()

public:
	ANLGuideLanterns();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "NL|Guide") float Spacing = 1100.f;
	// Yol maskesi yoksa ya da yol bulunamazsa nav yolundan bu kadar yana
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float SideOffset = 220.f;
	// Direk, yolun kenarından bu kadar dışarıda durur
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float EdgeMargin = 80.f;
	// Rıhtımda ve binanın dibinde fener dikilmez
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float SkipStart = 900.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float SkipEnd = 900.f;
	// Direk boyu çarpanı (1 = fener ~2.2 m'de, göz hizasının biraz üstünde)
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float PostHeightScale = 1.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float LanternIntensity = 2.6f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float LanternRadius = 650.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float GlassGlow = 0.3f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FLinearColor LampColor = FLinearColor(1.f, 0.6f, 0.28f);
	// Alev dalgalanmasının gücü (0 = sabit ışık)
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float FlickerStrength = 1.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FString SignText = TEXT("LIGHTHOUSE  >");

	void Build(const FVector& Start, const FVector& Goal);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Signs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> SignLabel;
	UPROPERTY() TObjectPtr<UStaticMesh> LanternMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> FlameMesh;
	UPROPERTY() TArray<FNLGuideLamp> Lamps;

private:
	// Patika maskesi (R kanalı), dünya XY'de [MaskMin, MaskMin + MaskSize] karesine yayılır
	TArray<uint8> Mask;
	int32 MaskW = 0, MaskH = 0;
	float MaskMin = -4671.25f, MaskSize = 9342.5f;
	bool LoadMask();
	float PathAt(const FVector& P) const;
	// Yola dik doğrultuda (N) yolun iki kenarını bul; yol yoksa false
	bool FindRoadEdges(const FVector& P, const FVector& N, float& OutNeg, float& OutPos) const;
};
