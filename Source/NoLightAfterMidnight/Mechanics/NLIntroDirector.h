// NoLightAfterMidnight - Açılış sinematiği: balıkçı teknesiyle adaya gelip rıhtıma yanaşma
// 1) Denizden geniş plan: yağmurda tekne adaya doğru gelir, şimşek çakar
// 2) Kaptan köşkünün içinden, kaptanın arkasından: dümeni çeviren kaptan, camlardan ada görünür
// 3) Rıhtımdan: tekne yavaşlayıp rıhtıma yanaşır, iskeleye hafifçe çarpar, motor rölantiye düşer
// 4) Kararma -> birinci şahıs: arka güvertede yanımızda bir yolcu; karakter rıhtıma adım atar, dönüp tekneye bakar
//    (yolcu el sallar), sonra adaya döner -> "DAY 1". Tekne motoru açıp denize çıkar ve gözden kaybolur.
// Boşluk / Enter / Esc ile geçilebilir (tekne yine ayrılır). Ölüp level yeniden açılınca tekne yoktur, oyuncu rıhtımda başlar.
// Level'e bir tane konur (Scripts/nl_setup_intro.py rıhtımın ölçülerini otomatik girer). Model: Scripts/blender_trawler.py
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
	// Tekne rıhtımın hangi yanına yanaşsın: 0 = otomatik (suyu derin olan taraf), 1 / -1 = sabit
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") int32 BoatSide = 0;
	// Teknenin yanaştığı yer: rıhtım ortasından denize doğru bu kadar ileride
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Dock") float MooringAlong = 450.f;

	// --- Zamanlama (sn) ---
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float ArriveTime = 16.f;      // teknenin yanaştığı an
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float ShotBTime = 6.5f;       // köşkün içinden plan başlar
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float ShotCTime = 11.5f;      // rıhtımdan plan başlar
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float StepOffDuration = 1.6f;
	// Oyuncu indikten sonra teknenin ayrılıp gözden kaybolması (sn)
	UPROPERTY(EditAnywhere, Category = "NL|Intro") float DepartDuration = 30.f;

	// --- Ses ---
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> EngineLoop;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> DockBumpSound;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> EngineStopSound;
	UPROPERTY(EditAnywhere, Category = "NL|Intro|Audio") TObjectPtr<USoundBase> StepOffSound;

	// Sinematiği oynat (GameMode çağırır; bitince GameMode'a 1. günü başlattırır)
	void Play();
	// Sinematiksiz başlangıç (ölüm sonrası / devam): tekne yok. bMovePlayer: oyuncu rıhtımda başlar
	void PlaceDocked(bool bMovePlayer);
	bool IsPlaying() const { return bPlaying; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> BoatRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> CineCamera;
	// Güverte çalışma lambası (bumba altında) ve köşkün içindeki lamba
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> BoatLantern;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> CabinLight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Wheel;
	UPROPERTY(Transient) TObjectPtr<class ANLSeatedFigure> Captain;
	UPROPERTY(Transient) TObjectPtr<class ANLSeatedFigure> Companion;
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
	float WheelAngle = 0.f;
	float LastYaw = 0.f;
	float LandTime = -1.f;

	// Ayrılış
	bool bDeparting = false;
	bool bGone = false;
	float DepartTime = 0.f;

	// Hesaplanan noktalar
	FVector Dir;        // rıhtım ekseni (denize doğru)
	FVector Side;       // teknenin yanaştığı taraf (rıhtımdan tekneye)
	FVector Moor;       // teknenin bağlandığı yer (su seviyesinde)
	FVector P0, P1, P2; // yaklaşma eğrisi (Bezier), P3 = Moor
	FVector StepFrom, StepTo;
	float StepYawFrom = 0.f, StepYawTo = 0.f;
	float StandY = 110.f;   // oyuncunun teknedeki yeri (rıhtım tarafı, yerel Y)

	TWeakObjectPtr<ANLPlayerCharacter> Player;

	void ComputePoints();
	UStaticMeshComponent* AddPart(const TCHAR* Name, class UStaticMesh* Mesh, FVector Loc, FRotator Rot, FVector Scale);
	void SetBoatVisible(bool bVisible);
	void SpawnCrew();
	void UpdateCrew(float DeltaSeconds);
	FVector PathAt(float U, FVector* OutTangent = nullptr) const;
	void UpdateBoat(float DeltaSeconds);
	void SetShot(int32 NewShot);
	void UpdateCamera(float DeltaSeconds);
	void StartFirstPerson();
	void Finish(bool bSkipped);
	void MovePlayerToDock();
	void StartDeparture();
	void UpdateDeparture(float DeltaSeconds);
	void ApplyBoatTransform();
};
