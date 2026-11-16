// NoLightAfterMidnight - Rıhtımdan deniz fenerine yol gösteren fenerli direkler
// Oyun başında NavMesh'ten rıhtımdan güvenlik masasına giden yolu bulur, yol boyunca aralıklarla yanan fenerli direkler dikilir.
// Yolun başına (rıhtımdan çıkınca) "LIGHTHOUSE" yazan bir tabela konur. GameMode, rıhtım sinematiği (NLIntroDirector)
// ve güvenlik masası olan level'de kendisi oluşturur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLGuideLanterns.generated.h"

class UInstancedStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLGuideLanterns : public AActor
{
	GENERATED_BODY()

public:
	ANLGuideLanterns();

	UPROPERTY(EditAnywhere, Category = "NL|Guide") float Spacing = 1100.f;
	// Yolun kenarına uzaklık
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float SideOffset = 180.f;
	// Rıhtımda ve binanın dibinde fener dikilmez
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float SkipStart = 900.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float SkipEnd = 900.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float LanternIntensity = 5.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") float LanternRadius = 700.f;
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FLinearColor LampColor = FLinearColor(1.f, 0.6f, 0.28f);
	UPROPERTY(EditAnywhere, Category = "NL|Guide") FString SignText = TEXT("LIGHTHOUSE  >");

	void Build(const FVector& Start, const FVector& Goal);

protected:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Posts;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Lanterns;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Signs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> SignLabel;
};
