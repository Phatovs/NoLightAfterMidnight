// NoLightAfterMidnight - E tuşuyla etkileşilen her şeyin arayüzü
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NLInteractable.generated.h"

class ANLPlayerCharacter;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UNLInteractable : public UInterface
{
	GENERATED_BODY()
};

class NOLIGHTAFTERMIDNIGHT_API INLInteractable
{
	GENERATED_BODY()

public:
	// Oyuncu E'ye bastığında çağrılır
	virtual void Interact(ANLPlayerCharacter* Player) {}

	// Ekranda gösterilecek ipucu ("Tamir et [E]" vb.)
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const { return FText::GetEmpty(); }

	virtual bool CanInteract(const ANLPlayerCharacter* Player) const { return true; }
};
