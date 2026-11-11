// NoLightAfterMidnight - Varsayılan asset'ler (sadece kurucu fonksiyonlarda kullanılır)
#pragma once

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"

/**
 * Sesler Content/Audio/NL/ klasöründen otomatik yüklenir; Details panelinden
 * başka bir ses seçerseniz sizinki kullanılır. Dosya yoksa alan boş kalır, oyun bozulmaz.
 * Kullanım: NL_DEFAULT_SOUND(FootstepSound, "footstep");
 */
#define NL_DEFAULT_SOUND(Prop, AssetName) \
	{ \
		static ConstructorHelpers::FObjectFinderOptional<USoundBase> NLSoundFinder(TEXT("/Game/Audio/NL/" AssetName "." AssetName)); \
		Prop = NLSoundFinder.Get(); \
	}
