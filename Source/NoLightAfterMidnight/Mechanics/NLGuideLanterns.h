// NoLightAfterMidnight - Rıhtımdan deniz fenerine yol gösteren fenerli direkler
// Oyun başında NavMesh'ten rıhtımdan güvenlik masasına giden yolu bulur, yol boyunca aralıklarla fenerli direkler dikilir.
// Direkler toprak yolun (Content/Data/NLPathMask.png, arazideki patika maskesiyle aynı) kenarına konur, yolun ortasına değil.
// Fenerler göz hizasına yakın, soluk yanar; ara ara titreyip kısa süre söner (korku havası).
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
	float NextFlicker = 0.f;   // bir sonraki titreme anı (oyun zamanı)
	float FlickerEnd = 0.f;    // titreme bitişi
	bool bBlackout = false;    // bu titremede tamamen sönüp geri gelir mi
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
	// Direk boyu (1 = 3.2 m). Fener göz hizasının biraz üstünde kalsın, oyuncu yürürken görsün.
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float PostHeightScale = 0.72f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float LanternIntensity = 2.6f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float LanternRadius = 650.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float GlassGlow = 0.3f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FLinearColor LampColor = FLinearColor(1.f, 0.6f, 0.28f);
	// Her fener bu aralıkta rastgele bir anda titrer (saniye)
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FVector2D FlickerInterval = FVector2D(7.f, 22.f);
	// Titremelerin bu kadarında fener birkaç saniye tamamen söner
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float BlackoutChance = 0.3f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FString SignText = TEXT("LIGHTHOUSE  >");

	void Build(const FVector& Start, const FVector& Goal);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Signs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> SignLabel;
	UPROPERTY() TObjectPtr<UStaticMesh> LanternMesh;
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
	void ScheduleFlicker(FNLGuideLamp& L, float Now, bool bFirst);
};
