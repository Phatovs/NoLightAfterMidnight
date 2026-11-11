#include "NLBodyRigComponent.h"
#include "TwoBoneIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace NLBones
{
	static const FName Pelvis(TEXT("pelvis"));
	static const FName Spine1(TEXT("spine_01"));
	static const FName Spine3(TEXT("spine_03"));
	static const FName Head(TEXT("head"));
	static const FName ThighL(TEXT("thigh_l")), CalfL(TEXT("calf_l")), FootL(TEXT("foot_l"));
	static const FName ThighR(TEXT("thigh_r")), CalfR(TEXT("calf_r")), FootR(TEXT("foot_r"));
	static const FName UpperArmL(TEXT("upperarm_l")), LowerArmL(TEXT("lowerarm_l")), HandL(TEXT("hand_l"));
	static const FName UpperArmR(TEXT("upperarm_r")), LowerArmR(TEXT("lowerarm_r")), HandR(TEXT("hand_r"));
}

UNLBodyRigComponent::UNLBodyRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Sürücünün animasyonu tamamlandıktan sonra çalış
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
}

void UNLBodyRigComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Driver) AddTickPrerequisiteComponent(Driver);
	if (AActor* Owner = GetOwner()) AddTickPrerequisiteActor(Owner);
}

void UNLBodyRigComponent::SetDriver(USkeletalMeshComponent* InDriver)
{
	Driver = InDriver;
	if (Driver && HasBegunPlay()) AddTickPrerequisiteComponent(Driver);
}

void UNLBodyRigComponent::SetCanState(bool bShow, float GrabAlpha, const FVector& InCanWorldLocation)
{
	bCanVisible = bShow;
	CanGrabAlpha = FMath::Clamp(GrabAlpha, 0.f, 1.f);
	CanWorldLocation = InCanWorldLocation;
}

bool UNLBodyRigComponent::HasBone(FName Bone) const
{
	return GetBoneIndex(Bone) != INDEX_NONE;
}

FVector UNLBodyRigComponent::BoneWorld(FName Bone)
{
	return GetComponentTransform().TransformPosition(GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace).GetLocation());
}

FVector UNLBodyRigComponent::ToCS(const FVector& World) const
{
	return GetComponentTransform().InverseTransformPosition(World);
}

void UNLBodyRigComponent::SolveLimb(FName Upper, FName Middle, FName End, const FVector& TargetWorld, const FVector& PoleWorld, float Alpha, const FQuat* KeepEndRotationCS)
{
	if (Alpha <= KINDA_SMALL_NUMBER || !HasBone(Upper) || !HasBone(Middle) || !HasBone(End)) return;

	FTransform A = GetBoneTransformByName(Upper, EBoneSpaces::ComponentSpace);
	FTransform B = GetBoneTransformByName(Middle, EBoneSpaces::ComponentSpace);
	FTransform C = GetBoneTransformByName(End, EBoneSpaces::ComponentSpace);

	const FVector Effector = FMath::Lerp(C.GetLocation(), ToCS(TargetWorld), FMath::Clamp(Alpha, 0.f, 1.f));
	AnimationCore::SolveTwoBoneIK(A, B, C, ToCS(PoleWorld), Effector, false, 1.0, 1.0);

	if (KeepEndRotationCS) C.SetRotation(*KeepEndRotationCS);

	SetBoneTransformByName(Upper, A, EBoneSpaces::ComponentSpace);
	SetBoneTransformByName(Middle, B, EBoneSpaces::ComponentSpace);
	SetBoneTransformByName(End, C, EBoneSpaces::ComponentSpace);
}

void UNLBodyRigComponent::RotateBoneWorldAxis(FName Bone, const FVector& AxisWorld, float Degrees)
{
	if (FMath::IsNearlyZero(Degrees) || !HasBone(Bone)) return;
	FTransform T = GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
	const FVector AxisCS = GetComponentTransform().InverseTransformVectorNoScale(AxisWorld).GetSafeNormal();
	T.SetRotation(FQuat(AxisCS, FMath::DegreesToRadians(Degrees)) * T.GetRotation());
	SetBoneTransformByName(Bone, T, EBoneSpaces::ComponentSpace);
}

void UNLBodyRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Driver || !GetSkinnedAsset() || !GetOwner()) return;

	using namespace NLBones;

	// 1) Yürüme/durma animasyonunu kopyala
	CopyPoseFromSkeletalComponent(Driver);

	const FVector Up = FVector::UpVector;
	const FRotator YawRot(0.f, GetOwner()->GetActorRotation().Yaw, 0.f);
	const FVector Fwd = YawRot.Vector();
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	const float Sit = FMath::Clamp(Input.SitAlpha, 0.f, 1.f);
	const float Crouch = FMath::Clamp(Input.CrouchAlpha, 0.f, 1.f);
	const float Work = FMath::Clamp(Input.WorkAlpha, 0.f, 1.f);
	const float Aim = FMath::Clamp(Input.FlashlightAim, 0.f, 1.f);

	const FVector SitFwd = Input.SitForward.GetSafeNormal2D().IsNearlyZero() ? Fwd : Input.SitForward.GetSafeNormal2D();
	const FVector SitRight = FVector::CrossProduct(Up, SitFwd);

	// 2) Kalça + bacaklar (oturma / eğilme). Ayaklar yerinde kalır ya da sandalyenin önüne gider.
	if (Sit > KINDA_SMALL_NUMBER || Crouch > KINDA_SMALL_NUMBER)
	{
		const FVector FootL0 = BoneWorld(FootL);
		const FVector FootR0 = BoneWorld(FootR);
		const FQuat FootLRot = GetBoneTransformByName(FootL, EBoneSpaces::ComponentSpace).GetRotation();
		const FQuat FootRRot = GetBoneTransformByName(FootR, EBoneSpaces::ComponentSpace).GetRotation();

		const FVector Pelvis0 = BoneWorld(Pelvis);
		FVector PelvisTarget = Pelvis0 - Up * (Input.CrouchDepth * Crouch) - Fwd * (8.f * Crouch);
		PelvisTarget = FMath::Lerp(PelvisTarget, Input.SitPelvis, Sit);

		FTransform PT = GetBoneTransformByName(Pelvis, EBoneSpaces::ComponentSpace);
		PT.SetLocation(ToCS(PelvisTarget));
		SetBoneTransformByName(Pelvis, PT, EBoneSpaces::ComponentSpace);

		const float FootLift = 9.f;
		FVector SitFootL = Input.SitPelvis + SitFwd * 44.f - SitRight * 14.f;
		FVector SitFootR = Input.SitPelvis + SitFwd * 44.f + SitRight * 14.f;
		SitFootL.Z = SitFootR.Z = Input.SitFloorZ + FootLift;

		const FVector TargetL = FMath::Lerp(FootL0, SitFootL, Sit);
		const FVector TargetR = FMath::Lerp(FootR0, SitFootR, Sit);
		const FVector KneeDir = FMath::Lerp(Fwd, SitFwd, Sit);

		SolveLimb(ThighL, CalfL, FootL, TargetL, (PelvisTarget + TargetL) * 0.5f + KneeDir * 100.f - Right * 10.f + Up * 20.f, 1.f, &FootLRot);
		SolveLimb(ThighR, CalfR, FootR, TargetR, (PelvisTarget + TargetR) * 0.5f + KneeDir * 100.f + Right * 10.f + Up * 20.f, 1.f, &FootRRot);
	}

	// Gövde öne eğilir (oturunca monitöre, eğilince yere, kalkarken masaya yaslanarak)
	RotateBoneWorldAxis(Spine1, Right, 8.f * Sit + 28.f * Crouch + Input.ExtraLean);

	// 3) Kollar
	const FVector ViewLoc = Input.View.GetLocation();
	const FQuat ViewQ = Input.View.GetRotation();
	const FVector ViewFwd = ViewQ.GetForwardVector();
	const FVector ViewRight = ViewQ.GetRightVector();
	const FVector ViewUp = ViewQ.GetUpVector();

	const FVector DeskBase = Input.SitPelvis + SitFwd * 56.f + FVector(0.f, 0.f, Input.SitFloorZ + Input.SitDeskHeight - Input.SitPelvis.Z);
	const float Desk = FMath::Max(Sit, FMath::Clamp(Input.DeskHandsAlpha, 0.f, 1.f));
	const float ReachR = FMath::Clamp(Input.ReachAlphaR, 0.f, 1.f);
	const float ReachL = FMath::Clamp(Input.ReachAlphaL, 0.f, 1.f);
	const float Pour = FMath::Clamp(Input.CanOverrideAlpha, 0.f, 1.f);

	// Benzin dökerken: sol el bidonun kulpunda, sağ el altından destekler
	const FVector PourHandle = Input.CanOverride.GetLocation();
	const FVector PourBottom = Input.CanOverride.TransformPosition(FVector(0.f, 0.f, -30.f));

	// Sabit referans: kapsül merkezi (yürüme animasyonuyla sallanmaz). Eğilince eller de iner.
	const FVector Base = GetOwner()->GetActorLocation() - Up * (Input.CrouchDepth * Crouch);
	// Boştaki eller yanda durur; yürüme salınımı büyük oranda bastırılır
	const float IdleHold = FMath::Clamp(Input.IdleArmHold, 0.f, 1.f);
	const FVector IdleR = Base + Fwd * 6.f + Right * 22.f - Up * 8.f;
	const FVector IdleL = Base + Fwd * 6.f - Right * 22.f - Up * 8.f;

	// Sağ el: fener. Kapalıyken göğsün altında önde (ışık aşağı-ileri), açıkken bakılan yöne kalkar.
	{
		const float A = Input.bHoldFlashlight ? 1.f : FMath::Max(IdleHold, FMath::Max3(Desk, Work, FMath::Max(ReachR, Pour)));
		if (A > KINDA_SMALL_NUMBER)
		{
			const FVector Lowered = Base + Fwd * 38.f + Right * 17.f + Up * 33.f;
			const FVector Aimed = ViewLoc + ViewFwd * 50.f + ViewRight * 16.f - ViewUp * 17.f;
			FVector T = Input.bHoldFlashlight ? FMath::Lerp(Lowered, Aimed, Aim) : FMath::Lerp(BoneWorld(HandR), IdleR, IdleHold);
			T = FMath::Lerp(T, DeskBase + SitRight * 17.f + SitFwd * (8.f * Aim * Sit), Desk);
			T = FMath::Lerp(T, ViewLoc + ViewFwd * 48.f + ViewRight * 13.f - ViewUp * 40.f, Work);
			T = FMath::Lerp(T, Input.ReachTargetR, ReachR);
			T = FMath::Lerp(T, PourBottom + Right * 6.f, Pour);

			const FVector Shoulder = BoneWorld(UpperArmR);
			const FVector PoleR = FMath::Lerp(Shoulder - Up * 60.f + Right * 40.f - Fwd * 25.f, Shoulder + Right * 80.f + Up * 10.f, FMath::Clamp(Input.ElbowsOut, 0.f, 1.f));
			SolveLimb(UpperArmR, LowerArmR, HandR, T, PoleR, A);
		}
	}

	// Sol el: masaya, işe, uzanılan noktaya ya da bidonun kulpuna
	{
		const float A = 1.f;
		if (A > KINDA_SMALL_NUMBER)
		{
			FVector T = FMath::Lerp(BoneWorld(HandL), IdleL, IdleHold);
			T = FMath::Lerp(T, DeskBase - SitRight * 17.f, Desk);
			T = FMath::Lerp(T, ViewLoc + ViewFwd * 48.f - ViewRight * 13.f - ViewUp * 40.f, Work);
			T = FMath::Lerp(T, Input.ReachTargetL + Up * 4.f, ReachL);
			T = FMath::Lerp(T, PourHandle + Up * 3.f, Pour);

			const FVector Shoulder = BoneWorld(UpperArmL);
			const FVector PoleL = FMath::Lerp(Shoulder - Up * 60.f - Right * 40.f - Fwd * 25.f, Shoulder - Right * 80.f + Up * 10.f, FMath::Clamp(Input.ElbowsOut, 0.f, 1.f));
			SolveLimb(UpperArmL, LowerArmL, HandL, T, PoleL, 1.f);
		}
	}

	// 4) Kafa gizli (kamera kafanın içinde kalmasın)
	if (bHideHead && HasBone(Head))
	{
		SetBoneScaleByName(Head, FVector(0.001f), EBoneSpaces::ComponentSpace);
	}

	RefreshBoneTransforms();
	PlaceHeldProps();

	// 5) Göz: kafa kemiğinin biraz önü/üstü. Aşağı bakınca boyun öne eğilmiş gibi göz öne kayar.
	if (HasBone(Head))
	{
		const float Pitch = FRotator::NormalizeAxis(Input.View.Rotator().Pitch);
		const float LookDown = FMath::Clamp(-FMath::Sin(FMath::DegreesToRadians(Pitch)), 0.f, 1.f);
		const FVector EyeWorld = BoneWorld(Head) + Fwd * (EyeForward + EyeForwardWhenLookingDown * LookDown) + Up * EyeUp;
		EyeActorSpace = GetOwner()->GetActorTransform().InverseTransformPosition(EyeWorld);
		bEyeValid = true;
	}
}

void UNLBodyRigComponent::PlaceHeldProps()
{
	using namespace NLBones;
	const FRotator YawRot(0.f, GetOwner()->GetActorRotation().Yaw, 0.f);
	const float Aim = FMath::Clamp(Input.FlashlightAim, 0.f, 1.f);
	const float Sit = FMath::Clamp(Input.SitAlpha, 0.f, 1.f);

	if (FlashlightPivot)
	{
		FlashlightPivot->SetVisibility(Input.bHoldFlashlight || Input.bFlashlightRest, true);
		if (!Input.bHoldFlashlight && Input.bFlashlightRest)
		{
			// Şarj istasyonunda yatıyor
			FlashlightPivot->SetWorldTransform(Input.FlashlightRest);
		}
		else if (Input.bHoldFlashlight)
		{
			const FQuat Lowered = FRotator(-35.f, YawRot.Yaw, 0.f).Quaternion();
			const FQuat Aimed = Input.View.GetRotation();
			const FQuat OnDesk = FRotator(-6.f, YawRot.Yaw, 0.f).Quaternion();
			FQuat R = FQuat::Slerp(Lowered, Aimed, Aim);
			R = FQuat::Slerp(R, OnDesk, Sit * (1.f - Aim));
			const FVector Hand = GetSocketLocation(HandR);
			FlashlightPivot->SetWorldLocationAndRotation(Hand + R.RotateVector(FVector(2.f, 0.f, -2.f)), R);
		}
	}

	if (CanPivot)
	{
		CanPivot->SetVisibility(bCanVisible, true);
		if (bCanVisible)
		{
			// Bidon gövdeden biraz dışarıda sallanır (bacağa girmesin)
			const FVector Outward = -FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
			const FVector HandLoc = GetSocketLocation(NLBones::HandL) - FVector(0.f, 0.f, 3.f) + Outward * 7.f;
			const FVector Carried = FMath::Lerp(CanWorldLocation, HandLoc, CanGrabAlpha);
			// Benzin dökerken bidon kaldırılıp eğilir
			const float Pour = FMath::Clamp(Input.CanOverrideAlpha, 0.f, 1.f);
			const FQuat Rot = FQuat::Slerp(YawRot.Quaternion(), Input.CanOverride.GetRotation(), Pour);
			CanPivot->SetWorldLocationAndRotation(FMath::Lerp(Carried, Input.CanOverride.GetLocation(), Pour), Rot);
		}
	}
}
