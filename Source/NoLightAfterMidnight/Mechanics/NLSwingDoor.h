// NoLightAfterMidnight - Menteşeli, açılıp kapanan kapı (E ile)
// Orijin = menteşenin altı; kanat mesh'i kapalıyken +X'e uzanır. OpenAngle kadar (Z ekseninde) döner.
// Yakına oyuncu olmayan bir karakter (Runner, Drowned...) gelirse kendiliğinden açılır: yapay zekâ kapıda takılmaz.
// Deniz feneri (ANLLighthouse) ev ve kule kapıları için oyun başında kendisi oluşturur.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLSwingDoor.generated.h"

class UStaticMeshComponent;
class USoundBase;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLSwingDoor : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLSwingDoor();
	virtual void Tick(float DeltaSeconds) override;

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

	void SetLeafMesh(class UStaticMesh* Mesh);
	void SetOpen(bool bNewOpen, bool bInstant = false);
	bool IsOpen() const { return bOpen; }

	// Açıkken dönüş açısı (derece, + = saat yönünün tersi / yerel Y'ye doğru)
	UPROPERTY(EditAnywhere, Category = "NL|Door") float OpenAngle = 95.f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float SwingTime = 0.7f;
	UPROPERTY(EditAnywhere, Category = "NL|Door") bool bStartOpen = false;
	UPROPERTY(EditAnywhere, Category = "NL|Door") float AutoOpenRadius = 170.f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> OpenSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> CloseSound;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Hinge;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Leaf;

private:
	bool bOpen = false;
	float Alpha = 0.f;   // 0 kapalı .. 1 açık
	float NpcCheck = 0.f;
};
