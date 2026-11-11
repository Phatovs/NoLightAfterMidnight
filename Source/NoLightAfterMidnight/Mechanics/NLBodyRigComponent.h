// NoLightAfterMidnight - Oyuncunun görünen gövdesi: animasyon + kodla verilen pozlar (oturma, eğilme, fener tutma)
#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "NLBodyRigComponent.generated.h"

/** Oyuncu her karede bu bilgileri doldurur; gövde düzeneği pozu buna göre kurar. Konumlar dünya uzayında. */
struct FNLBodyPoseInput
{
	FTransform View = FTransform::Identity;   // oyuncu kamerası

	// Oturma (0 = ayakta, 1 = oturmuş)
	float SitAlpha = 0.f;
	FVector SitPelvis = FVector::ZeroVector;  // kalçanın sandalyedeki yeri
	FVector SitForward = FVector::ForwardVector;
	float SitFloorZ = 0.f;
	float SitDeskHeight = 76.f;               // ellerin masadaki yüksekliği (zeminden)

	// Eğilme (yerden bir şey alırken)
	float CrouchAlpha = 0.f;
	float CrouchDepth = 50.f;

	// Sol el bir noktaya uzanır (bidonu alırken)
	float ReachAlphaL = 0.f;
	FVector ReachTargetL = FVector::ZeroVector;

	// Fener: elde mi, kaldırılmış mı (açıkken ileri tutulur)
	bool bHoldFlashlight = true;
	float FlashlightAim = 0.f;

	// Jeneratör tamiri: iki el öne
	float WorkAlpha = 0.f;

	// Sağ el bir noktaya uzanır (şalter, şarj istasyonu)
	float ReachAlphaR = 0.f;
	FVector ReachTargetR = FVector::ZeroVector;

	// Eller masaya bastırılır (otururken / kalkarken) ve gövde ekstra öne eğilir
	float DeskHandsAlpha = 0.f;
	float ExtraLean = 0.f;

	// Benzin dökerken bidon bu konum/açıya gelir, iki el bidonu tutar
	float CanOverrideAlpha = 0.f;
	FTransform CanOverride = FTransform::Identity;

	// Boştaki kolların yürüme salınımının ne kadar bastırılacağı (0 = animasyon serbest, 1 = sabit)
	float IdleArmHold = 0.85f;

	// Dirsekler aşağı yerine yana açılır (0-1). Bir şeyi iki yandan kavrarken.
	float ElbowsOut = 0.f;

	// Fener elde değilken (şarjda) durduğu yer
	bool bFlashlightRest = false;
	FTransform FlashlightRest = FTransform::Identity;
};

/**
 * Görünmeyen "sürücü" mesh yürüme animasyonunu oynatır, bu bileşen pozu her karede kopyalar
 * ve kalça/bacak/kolları IK ile istenen pozlara büker. Kafa gizlenir (birinci şahıs kamera içeride kalmasın).
 * Animasyon asset'i gerektirmez; gerçek animasyonlar geldiğinde sürücünün AnimBP'si değiştirilebilir.
 */
UCLASS(ClassGroup = (NL), meta = (BlueprintSpawnableComponent))
class NOLIGHTAFTERMIDNIGHT_API UNLBodyRigComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UNLBodyRigComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetDriver(USkeletalMeshComponent* InDriver);
	FNLBodyPoseInput& GetPoseInput() { return Input; }

	// Ele tutulan nesneler (her karede elin konumuna taşınır)
	void SetHeldFlashlight(USceneComponent* Pivot) { FlashlightPivot = Pivot; }
	void SetHeldCan(USceneComponent* Pivot) { CanPivot = Pivot; }

	// Bidon: 0 = dünyadaki yerinde (CanWorldLocation), 1 = sol elde
	void SetCanState(bool bShow, float GrabAlpha, const FVector& CanWorldLocation);

	// Kafayı gizle (birinci şahıs). Jumpscare/ölüm sahnesi gibi durumlarda kapatılabilir.
	UPROPERTY(EditAnywhere, Category = "NL|Body") bool bHideHead = true;

	// Gözün kafa kemiğine göre yeri: kamera her pozda (oturma, eğilme, yürüme) kafayı takip eder
	UPROPERTY(EditAnywhere, Category = "NL|Body") float EyeForward = 12.f;
	UPROPERTY(EditAnywhere, Category = "NL|Body") float EyeUp = 9.f;
	// Aşağı bakınca göz biraz öne gelir (boyun eğilir), böylece göğüs görüşü kapatmaz
	UPROPERTY(EditAnywhere, Category = "NL|Body") float EyeForwardWhenLookingDown = 16.f;

	// Gözün aktöre göre konumu (bir önceki kareden). Geçerli değilse false döner.
	bool GetEyeActorSpace(FVector& Out) const { Out = EyeActorSpace; return bEyeValid; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Driver;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> FlashlightPivot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> CanPivot;

	FNLBodyPoseInput Input;
	bool bCanVisible = false;
	float CanGrabAlpha = 0.f;
	FVector CanWorldLocation = FVector::ZeroVector;
	FVector EyeActorSpace = FVector::ZeroVector;
	bool bEyeValid = false;

	bool HasBone(FName Bone) const;
	FVector BoneWorld(FName Bone);
	FVector ToCS(const FVector& World) const;

	// İki kemikli IK: üst kemik, orta kemik, uç kemik; hedef ve dirsek/diz yönü dünya uzayında
	void SolveLimb(FName Upper, FName Middle, FName End, const FVector& TargetWorld, const FVector& PoleWorld, float Alpha, const FQuat* KeepEndRotationCS = nullptr);
	void RotateBoneWorldAxis(FName Bone, const FVector& AxisWorld, float Degrees);
	void PlaceHeldProps();
};
