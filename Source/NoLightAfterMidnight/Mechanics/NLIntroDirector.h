// NoLightAfterMidnight - Açılış sinematiği: botla adaya gelip rıhtıma yanaşma
// 1) Denizden geniş plan: yağmurda bot adaya doğru gelir, şimşek çakar
// 2) Botun arkasından: rıhtım ve kulübe yaklaşır
// 3) Rıhtımdan: bot yavaşlayıp rıhtıma yanaşır, iskeleye hafifçe çarpar, motor susar
// 4) Kararma -> birinci şahıs: karakter bottan rıhtıma adım atar, adaya döner -> "DAY 1"
// Boşluk / Enter / Esc ile geçilebilir. Ölüp level yeniden açılınca sinematik oynamaz, oyuncu rıhtımda başlar.
// Level'e bir tane konur (Scripts/nl_setup_intro.py rıhtımın ölçülerini otomatik girer).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NLIntroDirector.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class ANLPlayerCharacter;

UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	ANLIntroDirector();
	virtual void Tick(float DeltaSeconds) override;

	// --- Rıhtım (iskele üst yüzeyinin ortası, uzun ekseni, yarı boyları) ---
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") FVector DockCenter = FVector(-3793.f, 3169.f, 85.f);
	// Uzun eksenin yönü (yaw). Bu yönde ilerleyince denize çıkılır.
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") float DockYaw = 143.f;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") float DockHalfLength = 925.f;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") float DockHalfWidth = 310.f;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") float WaterZ = 12.f;
	// Bot rıhtımın hangi yanına yanaşsın: 0 = otomatik (suyu derin olan taraf), 1 / -1 = sabit
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") int32 BoatSide = 0;
	// Botun yanaştığı yer: rıhtım ortasından denize doğru bu kadar ileride
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") float MooringAlong = 450.f;

	// --- Zamanlama (sn) ---
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float ArriveTime = 16.f;      // botun yanaştığı an
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float ShotBTime = 6.5f;       // botun arkasından plan başlar
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float ShotCTime = 11.5f;      // rıhtımdan plan başlar
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float StepOffDuration = 1.6f;

	// --- Ses ---
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> EngineLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> DockBumpSound;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> EngineStopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> StepOffSound;

	// Sinematiği oynat (GameMode çağırır; bitince GameMode'a 1. günü başlattırır)
	void Play();
	// Sinematiksiz: bot rıhtımda bağlı durur. bMovePlayer: oyuncu rıhtımda başlar
	void PlaceDocked(bool bMovePlayer);
	bool IsPlaying() const { return bPlaying; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BoatRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CineCamera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> BoatLantern;
	UPROPERTY(Transient) TObjectPtr<class ANLSeatedFigure> Figure;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> EngineAudio;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> BoatAttenuation;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BoatParts;

	bool bPlaying = false;
	bool bComputed = false;
	float Time = 0.f;
	int32 Shot = -1;
	bool bBumped = false;
	bool bLightningDone = false;
	bool bFirstPerson = false;
	bool bStepStarted = false;
	bool bLanded = false;
	bool bFinished = false;
	float BumpKick = 0.f;
	float BobTime = 0.f;
	FVector BoatPos = FVector::ZeroVector;
	float BoatYaw = 0.f;

	// Hesaplanan noktalar
	FVector Dir;        // rıhtım ekseni (denize doğru)
	FVector Side;       // botun yanaştığı taraf
	FVector Moor;       // botun bağlandığı yer (su seviyesinde)
	FVector P0, P1, P2; // yaklaşma eğrisi (Bezier), P3 = Moor
	FVector StepFrom, StepTo;
	float StepYawFrom = 0.f, StepYawTo = 0.f;

	TWeakObjectPtr<ANLPlayerCharacter> Player;

	void ComputePoints();
	UStaticMeshComponent* AddPart(const TCHAR* Name, class UStaticMesh* Mesh, FVector Loc, FRotator Rot, FVector Scale);
	void ColorParts();
	void SetPilotVisible(bool bVisible);
	FVector PathAt(float U, FVector* OutTangent = nullptr) const;
	void UpdateBoat(float DeltaSeconds);
	void SetShot(int32 NewShot);
	void UpdateCamera(float DeltaSeconds);
	void StartFirstPerson();
	void Finish(bool bSkipped);
	void ApplyBoatTransform();
};
