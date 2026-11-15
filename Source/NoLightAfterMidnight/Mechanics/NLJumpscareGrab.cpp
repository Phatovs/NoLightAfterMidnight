#include "NLJumpscareGrab.h"
#include "Camera/CameraComponent.h"
#include "NLBodyRigComponent.h"
#include "AnimationRuntime.h"
#include "Engine/SkinnedAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

void FNLJumpscareGrab::Begin(AActor* Creature, const USkeletalMeshComponent* RefMesh, APlayerController* InVictim)
{
	bActive = true;
	Victim = InVictim;
	if (!Creature || !InVictim || !InVictim->PlayerCameraManager) return;

	// Bakışı yataya yakın düzelt (yere ya da tavana bakıyorsa yüz yine tam karşıda olsun)
	FRotator View = InVictim->GetControlRotation();
	View.Pitch = FMath::Clamp(FRotator::NormalizeAxis(View.Pitch), -8.f, 8.f);
	View.Roll = 0.f;
	InVictim->SetControlRotation(View);

	CamLoc = InVictim->PlayerCameraManager->GetCameraLocation();
	// Kamera yöneticisi bir kare geriden gelir (oyuncu o kare ışınlandıysa yanlış olur): oyuncunun kendi kamerasını kullan
	if (const APawn* VictimPawn = InVictim->GetPawn())
	{
		if (const UCameraComponent* Cam = VictimPawn->FindComponentByClass<UCameraComponent>()) CamLoc = Cam->GetComponentLocation();
	}
	BaseRot = View;
	const FRotator FaceRot(0.f, View.Yaw + 180.f, 0.f);

	// Kafanın aktöre göre konumu (referans pozdan; animasyondan bağımsız)
	FVector HeadOffset(0.f, 0.f, FallbackHeadHeight);
	if (RefMesh && RefMesh->GetSkinnedAsset())
	{
		const FReferenceSkeleton& RefSkel = RefMesh->GetSkinnedAsset()->GetRefSkeleton();
		const int32 HeadIndex = RefSkel.FindBoneIndex(HeadBone);
		if (HeadIndex != INDEX_NONE)
		{
			const FTransform HeadCS = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkel, HeadIndex);
			HeadOffset = RefMesh->GetRelativeTransform().TransformPosition(HeadCS.GetLocation());
		}
	}

	// Yüz bakış yönünde tam ekranın ortasında
	const FVector FaceTarget = CamLoc + View.Vector() * Distance - FVector(0.f, 0.f, FaceCenterOffset);
	StartLoc = FaceTarget - FaceRot.RotateVector(HeadOffset);
	Creature->SetActorLocationAndRotation(StartLoc, FaceRot, false, nullptr, ETeleportType::TeleportPhysics);
}

void FNLJumpscareGrab::Update(AActor* Creature, float Time, float T, FNLBodyPoseInput& In)
{
	if (!bActive || !Creature) return;

	// Kollar iki yana açılarak kalkar, sonra kapanıp kameranın iki yanını (kulak hizası) kavrar
	const float Rise = FMath::SmoothStep(0.f, 0.12f, T);
	const float Grab = FMath::SmoothStep(0.1f, 0.24f, T);
	const FVector CamFwd = BaseRot.Vector();
	const FVector CamRight = FRotationMatrix(BaseRot).GetUnitAxis(EAxis::Y);
	const FVector CamUp = FRotationMatrix(BaseRot).GetUnitAxis(EAxis::Z);

	const float Side = FMath::Lerp(50.f, 13.f, Grab);
	const float Ahead = FMath::Lerp(40.f, 12.f, Grab);
	const float Height = FMath::Lerp(-30.f, -4.f, Rise);
	const FVector Shake = (CamUp * FMath::Sin(Time * 38.f) * 1.6f + CamRight * FMath::Cos(Time * 31.f) * 1.2f) * Grab;

	// Yaratık oyuncuya dönük: onun sol eli oyuncunun sağında, sağ eli solunda (kollar çaprazlanmasın)
	In.ReachTargetL = CamLoc + CamFwd * Ahead + CamRight * Side + CamUp * Height + Shake;
	In.ReachTargetR = CamLoc + CamFwd * Ahead - CamRight * Side + CamUp * (Height + 3.f) - Shake;
	In.ReachAlphaL = In.ReachAlphaR = Rise;
	In.ElbowsOut = Rise;
	In.ExtraLean = 14.f * Rise;

	// Yüz öne atılır
	const FVector FlatFwd = FVector(CamFwd.X, CamFwd.Y, 0.f).GetSafeNormal();
	Creature->SetActorLocation(StartLoc + FlatFwd * (Lunge * Grab));

	// Yakalanmış gibi bakış sarsılır
	if (APlayerController* PC = Victim.Get())
	{
		const float S = CameraShake * Grab;
		PC->SetControlRotation(BaseRot + FRotator(FMath::Sin(Time * 47.f) * S, FMath::Cos(Time * 41.f) * S, FMath::Sin(Time * 29.f) * S * 0.6f));
	}
}
