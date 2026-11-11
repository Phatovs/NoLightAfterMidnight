// NoLightAfterMidnight - Ortak "yakalama" jumpscare'i (Boğulmuş ve Runner kullanır)
#pragma once

#include "CoreMinimal.h"

class AActor;
class APlayerController;
class USkeletalMeshComponent;
struct FNLBodyPoseInput;

/**
 * Yaratık oyuncunun kamerasının önüne ışınlanır (yüzü ekranın ortasında), kollarını iki yana açıp
 * kameranın iki yanını kavrar, yüzünü öne atar; tutarken eller titrer ve oyuncunun bakışı sarsılır.
 * Pozu NLBodyRigComponent'in giriş verisine yazar (montaj gerektirmez).
 */
struct NOLIGHTAFTERMIDNIGHT_API FNLJumpscareGrab
{
	float Distance = 55.f;           // yüz kameranın kaç cm önünde
	FName HeadBone = TEXT("head");
	float FaceCenterOffset = 10.f;   // kafa kemiği kafatasının dibinde; yüz ortası bu kadar yukarıda
	float FallbackHeadHeight = 75.f;
	float Lunge = 16.f;              // yakalayınca yüzün ne kadar daha yaklaştığı
	float CameraShake = 1.6f;        // bakış sarsıntısı (derece)

	// Yaratığı kameranın önüne koyar. Oyuncunun bakışını yataya yakın düzeltir ki yüz tam karşıda olsun.
	void Begin(AActor* Creature, const USkeletalMeshComponent* RefMesh, APlayerController* InVictim);

	// Time: başlangıçtan beri geçen sn, T: 0-1 ilerleme
	void Update(AActor* Creature, float Time, float T, FNLBodyPoseInput& In);

	bool IsActive() const { return bActive; }
	void Stop() { bActive = false; }

private:
	bool bActive = false;
	TWeakObjectPtr<APlayerController> Victim;
	FVector CamLoc = FVector::ZeroVector;
	FVector StartLoc = FVector::ZeroVector;
	FRotator BaseRot = FRotator::ZeroRotator;
};
