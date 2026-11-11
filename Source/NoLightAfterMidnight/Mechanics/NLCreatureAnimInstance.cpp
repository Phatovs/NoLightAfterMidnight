#include "NLCreatureAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UNLCreatureAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	if (!Character) return;

	Speed = Character->GetVelocity().Size2D();
	bIsMoving = Speed > 5.f;
	bIsFalling = Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling();
}
