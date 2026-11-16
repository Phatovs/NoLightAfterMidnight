// NoLightAfterMidnight - Oturan karakter (Unreal mankeni)
// Görünmez sürücü mesh boşta duruş animasyonunu oynatır; gövde düzeneği (NLBodyRigComponent) kalçayı oturağa,
// ayakları yere indirir, sağ eli bir hedefe (ör. motorun dümen kolu) uzatır, sol el dizinin üstünde durur.
// Orijin = oturağın üstünün ortası, +X = yüzünün baktığı yön. Bir şeye bağlanırsa (tekne) onunla sallanır.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLSeatedFigure.generated.h"

class UNLBodyRigComponent;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLSeatedFigure : public AActor
{
	GENERATED_BODY()

public:
	ANLSeatedFigure();
	virtual void Tick(float DeltaSeconds) override;

	// Oturaktan ayakların bastığı yere mesafe
	UPROPERTY(EditAnywhere, Category = "NL|Figure") float FloorDrop = 30.f;
	// Sağ el bu noktaya uzanır (aktörün yerel uzayı); bReachRight kapalıysa el serbest
	UPROPERTY(EditAnywhere, Category = "NL|Figure") bool bReachRight = true;
	UPROPERTY(EditAnywhere, Category = "NL|Figure") FVector RightHandTarget = FVector(30.f, 25.f, 14.f);
	// Hafifçe öne eğik, başı ileri bakar; Bakış yönü (yerel)
	UPROPERTY(EditAnywhere, Category = "NL|Figure") FRotator LookRotation = FRotator(-6.f, 0.f, 0.f);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Driver;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNLBodyRigComponent> Body;
};
