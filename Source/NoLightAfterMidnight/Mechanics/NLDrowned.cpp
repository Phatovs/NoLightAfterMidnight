#include "NLDrowned.h"
#include "NLDefaults.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

ANLDrowned::ANLDrowned()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(AppearSound, "drowned_appear");
	NL_DEFAULT_SOUND(BreathLoopSound, "drowned_breath_loop");
	NL_DEFAULT_SOUND(ScreechSound, "drowned_screech");
	NL_DEFAULT_SOUND(JumpscareSound, "jumpscare");

	// UE mankeni (Manny/Quinn) kapsülün altına ve ileri bakacak şekilde hizala
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

	PrimaryActorTick.bCanEverTick = false;
	AutoPossessAI = EAutoPossessAI::Disabled;

	BreathAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BreathAudio"));
	BreathAudio->SetupAttachment(GetMesh());
	BreathAudio->bAutoActivate = false;
}

void ANLDrowned::BeginPlay()
{
	Super::BeginPlay();

	// Drowned yürümez, kameralar arasında "belirir". Fizik/çarpışma gerekmez.
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	if (BreathLoopSound) BreathAudio->SetSound(BreathLoopSound);
	Vanish();
}

void ANLDrowned::AppearAt(const FTransform& SpawnTransform)
{
	// Spawn noktasından aşağı doğru zemini bul, ayaklar zemine bassın
	FVector Loc = SpawnTransform.GetLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NLDrownedGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 50.f), Loc - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		Loc = Hit.ImpactPoint;
	}
	Loc.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocationAndRotation(Loc, SpawnTransform.GetRotation().Rotator(), false, nullptr, ETeleportType::TeleportPhysics);

	SetActorHiddenInGame(false);
	bShown = true;

	UGameplayStatics::PlaySoundAtLocation(this, AppearSound, GetActorLocation());
	BreathAudio->Play();
	PlayMontageSafe(AppearMontage, 0.f);
}

float ANLDrowned::PlayCoverFace()
{
	// Kamerada izlenirken duyulsun diye 2D
	UGameplayStatics::PlaySound2D(this, ScreechSound);
	return PlayMontageSafe(CoverFaceMontage, FallbackCoverFaceDuration);
}

float ANLDrowned::PlayFlee()
{
	return PlayMontageSafe(FleeMontage, FallbackFleeDuration);
}

void ANLDrowned::Vanish()
{
	bShown = false;
	SetActorHiddenInGame(true);
	BreathAudio->Stop();
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->StopAllMontages(0.f);
	}
}

float ANLDrowned::PerformJumpscare(APlayerController* Victim)
{
	if (Victim && Victim->PlayerCameraManager)
	{
		const FVector CamLoc = Victim->PlayerCameraManager->GetCameraLocation();
		const FRotator CamRot = Victim->PlayerCameraManager->GetCameraRotation();
		const FVector Forward = FRotator(0.f, CamRot.Yaw, 0.f).Vector();

		FVector Loc = CamLoc + Forward * JumpscareDistance;
		Loc.Z = CamLoc.Z + JumpscareHeightOffset + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FRotator FaceRot(0.f, CamRot.Yaw + 180.f, 0.f);

		SetActorLocationAndRotation(Loc, FaceRot, false, nullptr, ETeleportType::TeleportPhysics);
	}

	SetActorHiddenInGame(false);
	bShown = true;
	BreathAudio->Stop();
	UGameplayStatics::PlaySound2D(this, JumpscareSound);
	return PlayMontageSafe(JumpscareMontage, FallbackJumpscareDuration);
}

float ANLDrowned::PlayMontageSafe(UAnimMontage* Montage, float Fallback)
{
	if (Montage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			const float Len = Anim->Montage_Play(Montage);
			if (Len > 0.f) return Len;
		}
	}
	return Fallback;
}
