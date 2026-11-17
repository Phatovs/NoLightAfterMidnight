// NoLightAfterMidnight - Deniz feneri
// Taş kaide, yukarı daralan beyaz/kırmızı bantlı kule, açık kapılı zemin katı (içinde asılı lamba, sandık, varil),
// küçük pencereler, korkuluklu balkon, camlı fener odası ve kubbe.
// Tepede dönen iki ışık huzmesi: denize vurur, sisin içinde görünür, uzaktan yön gösterir.
// Elektrik kesilince (jeneratör/şalter) fener ve iç lamba söner, elektrik gelince yeniden yanar.
// bBuildTower=false: sadece dönen ışık (başka bir kule modelinin tepesine konur).
// Yerel eksenler: orijin = kule tabanının ortası (zemin), +X = kapının baktığı yön.
#pragma once

#include "CoreMinimal.h"
#include "NLLightStutter.h"
#include "GameFramework/Actor.h"
#include "NLLighthouse.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLLighthouse : public AActor
{
	GENERATED_BODY()

public:
	ANLLighthouse();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "NL")
	void RebuildNow() { Layout(); }

	// --- Kule ---
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse") bool bBuildTower = true;
	// Kulenin gövde yüksekliği (balkon bunun üstünde)
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse") float TowerHeight = 1000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse") float BaseRadius = 260.f;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse") bool bInteriorProps = false;
	// Açık kapı kanadı, kasa ve basamak (kule bir binaya bitişikse kapatılır: kapı boşluğu binaya açılır)
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse") bool bDoorPanel = true;

	// --- Işık ---
	// Bir tam dönüş süresi (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float RotationPeriod = 16.f;
	// Huzmenin aşağı eğimi (denize vursun)
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float BeamPitch = -7.f;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float BeamIntensity = 2500.f;      // kandela
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float BeamRange = 22000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float BeamAngle = 7.f;
	// Görünen huzme (sisli havada ışık çizgisi) parlaklığı ve uzunluğu
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float BeamGlow = 0.12f;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float BeamLength = 6000.f;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") FLinearColor BeamColor = FLinearColor(1.f, 0.66f, 0.28f);   // eski akkor lamba: hafif sarı
	// Elektrik yokken söner
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") bool bRequirePower = true;
	UPROPERTY(EditAnywhere, Category = "NL|Lighthouse|Light") float InteriorLampIntensity = 6.f;

	bool IsLit() const { return LitAlpha > 0.5f; }
	void SetStutterInterval(const FVector2D& Range) { Stutter = FNLLightStutter(); Stutter.Interval = Range; }

	// Ana menü: huzme kendi dönmez, verilen noktaya (fare imleci) döner; görünen huzme o noktada biter
	void AimAt(const FVector& WorldPoint, float Speed = 6.f);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Base;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Tower;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Gallery;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> LanternRoom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Boards;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Windows;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Props;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> PropBarrels;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> InteriorLantern;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> InteriorLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Rotor;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Lens;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BeamA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BeamB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> SpotA;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> SpotB;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> RoomLight;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> BeamMIDs;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> LensMID;
	float LitAlpha = 1.f;
	// Eski lamba: ara ara tekler (oyunda ve menüde)
	FNLLightStutter Stutter;
	float StutterMul = 1.f;
	bool bManualAim = false;
	FVector AimPoint = FVector::ZeroVector;
	float AimSpeed = 6.f;
	float CeilingZ = 300.f;

	void Layout();
	void ApplyLit(float Alpha);
	float RadiusAt(float Z) const;
};
