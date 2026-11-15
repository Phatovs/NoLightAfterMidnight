// NoLightAfterMidnight - Sigorta kutusu etkileşimi: kutunun 3D modeli üstünde fareyle sigorta taşıma ve şalter
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NLFuseBoxWidget.generated.h"

class ANLFuseBox;
class ANLPlayerCharacter;
class UTextBlock;

/**
 * Ayrı bir panel çizmez: oyuncu E'ye basınca görüş kutunun yakın plan kamerasına geçer.
 * Fare her karede oyunun girişinden okunur (tıklamaları bu katman yutmaz, takılı kalma olmaz):
 *  - sigortaya basılı tut, sürükle, başka yuvaya bırak -> yer değiştirir (sadece tıklamak bir şey yapmaz)
 *  - şalter koluna basılı tut, aşağı/yukarı çek -> yarıyı geçip bırakınca o tarafa geçer
 *  - şalter açıkken sigortayı tutmaya çalışmak -> elektrik çarpar
 * Altta küçük bir ipucu satırı vardır. E ile çıkılır.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API UNLFuseBoxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(ANLFuseBox* InBox, ANLPlayerCharacter* InPlayer);

	// Testler de kullanır
	// Sigortayı tutmaya çalış: şalter açıksa elektrik çarpar ve false döner
	bool TryPickFuse(int32 SlotIdx);
	void DropFuse(int32 TargetSlot);
	void ToggleSwitch();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TWeakObjectPtr<ANLFuseBox> Box;
	TWeakObjectPtr<ANLPlayerCharacter> Player;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;

	int32 DragSlot = INDEX_NONE;
	bool bMouseWasDown = false;
	bool bLeverDrag = false;
	float LeverStartMouseY = 0.f;
	float LeverStartAngle = 0.f;

	void Build();
	void UpdateMouse();
	void UpdateStatus();
};
