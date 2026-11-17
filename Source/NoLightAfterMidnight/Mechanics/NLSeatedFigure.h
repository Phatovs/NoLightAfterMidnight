// NoLightAfterMidnight - Oturan ya da ayakta duran karakter (Unreal mankeni)
// Görünmez sürücü mesh boşta duruş animasyonunu oynatır; gövde düzeneği (NLBodyRigComponent) üstüne pozlar:
//  - Oturan (bSeated): kalça oturağa, ayaklar yere; sağ el bir hedefe (ör. dümen kolu), sol el dizin üstünde. Orijin = oturağın üstü.
//  - Ayakta (bSeated=false): orijin kalça hizasında (ayaklar StandHipHeight kadar aşağıda), elleri dünya hedeflerine uzanır
//    (ör. iki el dümende) ya da yanda durur. WaveAlpha > 0 iken sağ el başın yanında sallanır (el sallama).
// +X = yüzünün baktığı yön. Bir şeye bağlanırsa (tekne) onunla sallanır.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLSeatedFigure.generated.h"

class UNLBodyRigComponent;
class UMaterialInterface;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLSeatedFigure : public AActor
{
	GENERATED_BODY()

public:
	ANLSeatedFigure();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "NL|Figure") bool bSeated = true;
	// Oturan: oturaktan ayakların bastığı yere mesafe
	UPROPERTY(EditAnywhere, Category = "NL|Figure") float FloorDrop = 30.f;
	// Ayakta: kalça yüksekliği (aktör orijini zeminden bu kadar yukarıda)
	UPROPERTY(EditAnywhere, Category = "NL|Figure") float StandHipHeight = 92.f;
	// Sağ el bu noktaya uzanır (aktörün yerel uzayı); bReachRight kapalıysa el serbest
	UPROPERTY(EditAnywhere, Category = "NL|Figure") bool bReachRight = true;
	UPROPERTY(EditAnywhere, Category = "NL|Figure") FVector RightHandTarget = FVector(30.f, 25.f, 14.f);
	// Hafifçe öne eğik, başı ileri bakar; Bakış yönü (yerel)
	UPROPERTY(EditAnywhere, Category = "NL|Figure") FRotator LookRotation = FRotator(-6.f, 0.f, 0.f);
	// Kıyafet (boşsa mankenin kendi malzemesi)
	UPROPERTY(EditAnywhere, Category = "NL|Figure") TObjectPtr<UMaterialInterface> Clothing;

	// Elleri dünya koordinatındaki hedeflere uzat (her kare çağrılabilir). Alpha 0 = el serbest.
	void SetHandTargetsWorld(float AlphaR, const FVector& R, float AlphaL, const FVector& L);
	// 0..1: sağ el kalkar ve sallanır
	void SetWave(float Alpha) { WaveAlpha = FMath::Clamp(Alpha, 0.f, 1.f); }
	void SetClothing(UMaterialInterface* M);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Driver;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNLBodyRigComponent> Body;

private:
	bool bWorldTargets = false;
	float WorldAlphaR = 0.f, WorldAlphaL = 0.f;
	FVector WorldR = FVector::ZeroVector, WorldL = FVector::ZeroVector;
	float WaveAlpha = 0.f;
	float WaveTime = 0.f;
};
