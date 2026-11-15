// NoLightAfterMidnight - Ortak tipler ve delegeler
#pragma once

#include "CoreMinimal.h"
#include "NLTypes.generated.h"

// Oyunun neden bittiği (HUD ve ses seçimi için)
UENUM(BlueprintType)
enum class ENLGameOverReason : uint8
{
	Drowned UMETA(DisplayName = "Drowned"),
	Runner  UMETA(DisplayName = "Runner")
};

// Drowned yapay zekasının durumları
UENUM(BlueprintType)
enum class ENLDrownedState : uint8
{
	Inactive,   // Gün aktif değil
	Waiting,    // Bir sonraki kamerada belirmeyi bekliyor
	Watching,   // Bir kamerada duruyor, oyuncunun ışık tutması bekleniyor
	Flinching,  // Işık yedi: elini yüzüne kapatıyor / kaçıyor
	Away,       // 3 kaçıştan sonra haritadan ayrıldı (2 dk)
	Attacking   // Işık tutulmadı -> saldırı / jumpscare
};

// Oyuncunun o anki modu
UENUM(BlueprintType)
enum class ENLPlayerMode : uint8
{
	Walking,
	Seated,
	Repairing,
	UsingFuseBox,  // sigorta kutusu arayüzü açık
	HoldingDoor,   // Runner'a karşı saklanma kapısını tutuyor
	Dead
};

class ANLSecurityCamera;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNLOnPowerChanged, bool, bHasPower);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNLOnDayEvent, int32, Day);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNLOnGameOver, ENLGameOverReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNLOnSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNLOnCameraFlashed, ANLSecurityCamera*, Camera);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNLOnCameraChanged, int32, CameraIndex);
