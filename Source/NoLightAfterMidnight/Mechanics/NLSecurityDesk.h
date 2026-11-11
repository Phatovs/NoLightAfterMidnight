// NoLightAfterMidnight - Masa + bilgisayar: kamera akışı ve harita
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLInteractable.h"
#include "NLTypes.h"
#include "NLSecurityDesk.generated.h"

class ANLSecurityCamera;
class UCameraComponent;
class UStaticMeshComponent;
class UArrowComponent;
class UAudioComponent;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UNLPowerConsumerComponent;
class USoundBase;

/**
 * Oyuncu buraya oturur (E). Görüş masanın ViewCamera'sına yumuşakça geçer.
 * Monitör materyali aktif kameranın RenderTarget'ını gösterir.
 * Elektrik giderse monitör kararır, kamera değiştirme ve ışık çalışmaz.
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLSecurityDesk : public AActor, public INLInteractable
{
	GENERATED_BODY()

public:
	ANLSecurityDesk();

	virtual void Interact(ANLPlayerCharacter* Player) override;
	virtual FText GetInteractPrompt(const ANLPlayerCharacter* Player) const override;

	UFUNCTION(BlueprintCallable, Category = "NL|Desk") void SwitchToCamera(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "NL|Desk") void NextCamera();
	UFUNCTION(BlueprintCallable, Category = "NL|Desk") void PreviousCamera();

	// SPACE: aktif kameranın altındaki ışığı yakar
	UFUNCTION(BlueprintCallable, Category = "NL|Desk") bool FlashActiveCamera();

	UFUNCTION(BlueprintPure, Category = "NL|Desk") ANLSecurityCamera* GetActiveCamera() const;
	UFUNCTION(BlueprintPure, Category = "NL|Desk") int32 GetActiveIndex() const { return ActiveIndex; }
	UFUNCTION(BlueprintPure, Category = "NL|Desk") bool IsPowered() const;

	const TArray<TObjectPtr<ANLSecurityCamera>>& GetCameras() const { return Cameras; }
	FTransform GetSeatTransform() const;
	float GetViewBlendTime() const { return ViewBlendTime; }
	UTextureRenderTarget2D* GetFeedRenderTarget() const { return FeedRenderTarget; }

	void OnPlayerSeated();
	void OnPlayerLeft();

	UPROPERTY(BlueprintAssignable, Category = "NL|Desk")
	FNLOnCameraChanged OnActiveCameraChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> DeskMesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UStaticMeshComponent> MonitorMesh;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UCameraComponent> ViewCamera;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UArrowComponent> SeatPoint;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UAudioComponent> MonitorAudio;
	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLPowerConsumerComponent> PowerConsumer;

	// Sahnedeki 8 kamerayı sırayla buraya ekleyin (Cam01..Cam08)
	UPROPERTY(EditInstanceOnly, Category = "NL|Desk") TArray<TObjectPtr<ANLSecurityCamera>> Cameras;

	// Boş bırakılırsa oyun başında otomatik oluşturulur
	UPROPERTY(EditAnywhere, Category = "NL|Desk") TObjectPtr<UTextureRenderTarget2D> FeedRenderTarget;
	UPROPERTY(EditAnywhere, Category = "NL|Desk") FIntPoint AutoFeedResolution = FIntPoint(1280, 720);
	UPROPERTY(EditAnywhere, Category = "NL|Desk") int32 MonitorMaterialIndex = 0;
	UPROPERTY(EditAnywhere, Category = "NL|Desk") FName FeedTextureParam = TEXT("FeedTexture");
	UPROPERTY(EditAnywhere, Category = "NL|Desk") FName PowerParam = TEXT("Power");
	UPROPERTY(EditAnywhere, Category = "NL|Desk") float ViewBlendTime = 0.8f;

	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> CameraSwitchSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ComputerOnSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> ComputerOffSound;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> MonitorHumLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Audio") TObjectPtr<USoundBase> SitDownSound;

	UFUNCTION()
	void HandlePowerChanged(bool bHasPower);

private:
	int32 ActiveIndex = 0;
	bool bPlayerSeated = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MonitorMID;

	void RefreshFeed();
};
