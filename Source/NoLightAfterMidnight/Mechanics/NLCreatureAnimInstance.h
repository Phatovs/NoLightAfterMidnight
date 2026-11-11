// NoLightAfterMidnight - Drowned / Runner / oyuncu Animation Blueprint'leri için ortak üst sınıf
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NLCreatureAnimInstance.generated.h"

/**
 * AnimBP'nin Class Settings > Parent Class'ını bu yapın.
 * State machine'de Speed ve bIsFalling'i kullanın (Idle <-> Walk/Run blend).
 * Montajlar (yüz kapatma, saldırı, oturma...) C++'tan "DefaultSlot" üzerinden oynatılır;
 * AnimGraph'ta state machine çıkışını bir "Slot 'DefaultSlot'" düğümünden geçirmeyi unutmayın.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLCreatureAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "NL|Anim") float Speed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "NL|Anim") bool bIsFalling = false;
	UPROPERTY(BlueprintReadOnly, Category = "NL|Anim") bool bIsMoving = false;
};
