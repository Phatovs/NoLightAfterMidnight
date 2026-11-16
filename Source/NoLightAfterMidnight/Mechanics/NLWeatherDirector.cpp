#include "NLWeatherDirector.h"
#include "NLDefaults.h"
#include "NLGameMode.h"
#include "NLAmbienceDirector.h"
#include "NLPlayerCharacter.h"
#include "NLGameSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

static float AmbVolOf(const UObject* O) { const UNLGameSubsystem* S = UNLGameSubsystem::Get(O); return S ? S->GetAmbienceVolume() : 1.f; }
#define AmbVol() AmbVolOf(this)

ANLWeatherDirector::ANLWeatherDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;   // kamera bu karede yerini aldıktan sonra
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RainDrops"));
	Drops->SetupAttachment(RootComponent);
	Drops->SetUsingAbsoluteLocation(true);
	Drops->SetUsingAbsoluteRotation(true);
	Drops->SetUsingAbsoluteScale(true);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCanEverAffectNavigation(false);
	Drops->CastShadow = false;
	Drops->bReceivesDecals = false;
	Drops->SetMobility(EComponentMobility::Movable);
	Drops->SetVisibility(false);
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Get()) Drops->SetStaticMesh(Cube.Get());
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> RainMat(TEXT("/Game/FX/NL/M_NLRainSoft.M_NLRainSoft"));
	RainMaterial = RainMat.Get();

	// Şimşek: normalde kapalı, çakınca bir an tüm adayı aydınlatan ikinci bir yönlü ışık
	Flash = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("LightningFlash"));
	Flash->SetupAttachment(RootComponent);
	Flash->SetUsingAbsoluteRotation(true);
	Flash->SetWorldRotation(FRotator(-58.f, 35.f, 0.f));
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->SetIntensity(0.f);
	Flash->SetLightColor(FLinearColor(0.72f, 0.8f, 1.f));
	Flash->SetCastShadows(false);
	Flash->SetAtmosphereSunLight(false);
	Flash->ForwardShadingPriority = -10;   // sahnenin asıl ışığı (ay) öncelikli kalsın
	Flash->SetVisibility(false);

	NL_DEFAULT_SOUND(RainLoop, "amb_rain_loop");
	NL_DEFAULT_SOUND(RainRoofLoop, "amb_rain_roof_loop");
	static const TCHAR* ThunderPaths[] = {
		TEXT("/Game/Audio/NL/thunder_near.thunder_near"), TEXT("/Game/Audio/NL/thunder_far.thunder_far"),
		TEXT("/Game/Audio/NL/thunder_far2.thunder_far2") };
	for (const TCHAR* Path : ThunderPaths)
	{
		ConstructorHelpers::FObjectFinderOptional<USoundBase> Finder(Path);
		if (Finder.Get()) ThunderSounds.Add(Finder.Get());
	}
}

void ANLWeatherDirector::BeginPlay()
{
	Super::BeginPlay();

	if (RainMaterial)
	{
		DropMID = UMaterialInstanceDynamic::Create(RainMaterial, this);
		Drops->SetMaterial(0, DropMID);
		DropMID->SetScalarParameterValue(TEXT("Opacity"), DropOpacity);
		DropMID->SetScalarParameterValue(TEXT("Glow"), DropGlow);
	}

	// Damlalar: hepsi aynı yöne (rüzgârla eğik) bakan ince uzun kutular
	DropData.SetNum(FMath::Max(DropCount, 0));
	DropXforms.SetNum(DropData.Num());
	TArray<FTransform> Init;
	Init.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), DropData.Num());
	Drops->AddInstances(Init, false, true);
	for (FDrop& D : DropData) { D.Pos = FVector::ZeroVector; D.Speed = 0.f; D.KillZ = 0.f; }

	if (AExponentialHeightFog* FogActor = TActorIterator<AExponentialHeightFog>(GetWorld()) ? *TActorIterator<AExponentialHeightFog>(GetWorld()) : nullptr)
	{
		Fog = FogActor->GetComponent();
		if (Fog) BaseFogDensity = Fog->FogDensity;
	}

	RainAudio = UGameplayStatics::SpawnSound2D(this, RainLoop, 0.f, 1.f, 0.f, nullptr, true, false);
	RoofAudio = UGameplayStatics::SpawnSound2D(this, RainRoofLoop, 0.f, 1.f, 0.f, nullptr, true, false);
	if (RainAudio) RainAudio->SetVolumeMultiplier(0.001f);
	if (RoofAudio) RoofAudio->SetVolumeMultiplier(0.001f);

	if (ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		GM->OnDayStarted.AddDynamic(this, &ANLWeatherDirector::HandleDayStarted);
	}
	// 1. gece yağmurluysa açılış sinematiğinde de yağsın
	// Devam ile başka bir gece açılıyorsa o gecenin havası
	const AGameModeBase* GMB = GetWorld()->GetAuthGameMode();
	const int32 StartNight = FMath::Max(1, UGameplayStatics::GetIntOption(GMB ? GMB->OptionsString : FString(), TEXT("NLDay"), 1));
	if (RainyDays.Contains(StartNight)) SetRaining(true, true);
}

void ANLWeatherDirector::HandleDayStarted(int32 Day)
{
	const bool bRain = RainyDays.Contains(Day);
	// İlk gece oyun zaten yağmurla açılır; sonraki gecelerde yavaşça başlar/diner
	SetRaining(bRain, Day == 1);
}

void ANLWeatherDirector::SetRaining(bool bOn, bool bInstant)
{
	if (bOn && !bRaining)
	{
		// Damlaları alana dağıt
		const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const FVector Center = Cam ? Cam->GetCameraLocation() : GetActorLocation();
		for (FDrop& D : DropData) RespawnDrop(D, Center, true);
		LightningTimer = FirstLightningDelay;
	}
	bRaining = bOn;
	if (bInstant) Intensity = bOn ? 1.f : 0.f;
	UE_LOG(LogTemp, Log, TEXT("NLWeather: rain %s%s (%d drops, material %s, %d thunder sounds)"),
		bOn ? TEXT("ON") : TEXT("OFF"), bInstant ? TEXT(" instantly") : TEXT(""), DropData.Num(), *GetNameSafe(RainMaterial), ThunderSounds.Num());
}

void ANLWeatherDirector::RespawnDrop(FDrop& D, const FVector& Center, bool bAnyHeight)
{
	const float Top = Center.Z + AreaHeight * 0.5f;
	const float Bottom = Center.Z - AreaHeight * 0.5f;
	D.Pos.X = Center.X + FMath::FRandRange(-AreaRadius, AreaRadius);
	D.Pos.Y = Center.Y + FMath::FRandRange(-AreaRadius, AreaRadius);
	D.Speed = FMath::FRandRange(0.85f, 1.15f);

	// Çatı/zemin: yukarıdan aşağı bak (oyuncu binadaysa çatı alanın üstünde kalabilir, o yüzden yüksekten)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLRain), false, UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector From(D.Pos.X, D.Pos.Y, Center.Z + 6000.f);
	const FVector To(D.Pos.X, D.Pos.Y, Bottom - 200.f);
	D.KillZ = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q) ? Hit.ImpactPoint.Z : Bottom - 200.f;

	const float Low = FMath::Max(D.KillZ, Bottom);
	if (Low >= Top)
	{
		// Üstü kapalı (bina içi): bu tur gizli kalır, kısa süre sonra başka yerde dener
		D.Pos.Z = Low;
		D.KillZ = Top + AreaHeight;   // işaret: gizli
		return;
	}
	D.Pos.Z = bAnyHeight ? FMath::FRandRange(Low, Top) : Top + FMath::FRandRange(0.f, 250.f);
}

void ANLWeatherDirector::UpdateDrops(float DeltaSeconds, const FVector& Center)
{
	const FVector FallVel = Wind + FVector(0.f, 0.f, -FallSpeed);
	const FQuat Rot = FRotationMatrix::MakeFromZ(-FallVel.GetSafeNormal()).ToQuat();
	const FVector Scale(DropWidth / 100.f, DropWidth / 100.f, DropLength / 100.f);
	const FTransform Hidden(FQuat::Identity, Center, FVector::ZeroVector);
	const float Top = Center.Z + AreaHeight * 0.5f;
	const float Bottom = Center.Z - AreaHeight * 0.5f;
	const float R2 = AreaRadius * 2.f;
	int32 Traces = 0;

	for (int32 i = 0; i < DropData.Num(); ++i)
	{
		FDrop& D = DropData[i];
		const bool bSleeping = D.KillZ > D.Pos.Z + 1.f && D.KillZ >= Top;
		if (bSleeping)
		{
			// Gizli damla: düşüyormuş gibi zaman geçir, sonra başka yerde dene
			D.Pos.Z -= FallSpeed * D.Speed * DeltaSeconds;
			if (D.Pos.Z < Bottom) { RespawnDrop(D, Center, false); ++Traces; }
			DropXforms[i] = Hidden;
			continue;
		}

		D.Pos += FallVel * D.Speed * DeltaSeconds;

		// Oyuncu yürüdükçe alanın dışında kalan damlalar karşı kenara sarılır
		bool bWrapped = false;
		if (D.Pos.X < Center.X - AreaRadius) { D.Pos.X += R2; bWrapped = true; }
		else if (D.Pos.X > Center.X + AreaRadius) { D.Pos.X -= R2; bWrapped = true; }
		if (D.Pos.Y < Center.Y - AreaRadius) { D.Pos.Y += R2; bWrapped = true; }
		else if (D.Pos.Y > Center.Y + AreaRadius) { D.Pos.Y -= R2; bWrapped = true; }
		if (bWrapped || D.Pos.Z > Top + 600.f)
		{
			RespawnDrop(D, Center, true);
			++Traces;
		}
		else if (D.Pos.Z < D.KillZ || D.Pos.Z < Bottom)
		{
			RespawnDrop(D, Center, false);
			++Traces;
		}

		if (D.KillZ >= Top) { DropXforms[i] = Hidden; continue; }
		DropXforms[i] = FTransform(Rot, D.Pos, Scale);
	}
	Drops->BatchUpdateInstancesTransforms(0, DropXforms, true, true, true);
}

void ANLWeatherDirector::TriggerLightning()
{
	FlashTime = 0.f;
	FlashPulses.Reset();
	FlashPulses.Add(0.f);
	FlashPulses.Add(FMath::FRandRange(0.12f, 0.2f));
	if (FMath::FRand() < 0.6f) FlashPulses.Add(FMath::FRandRange(0.3f, 0.45f));
	Flash->SetWorldRotation(FRotator(FMath::FRandRange(-70.f, -40.f), FMath::FRandRange(0.f, 360.f), 0.f));
	Flash->SetVisibility(true);

	// Gök gürültüsü: şimşek ne kadar uzaksa o kadar geç ve kısık gelir
	const float Far = FMath::FRand();
	PendingThunderTime = 0.4f + Far * 3.2f;
	PendingThunderVol = FMath::Lerp(1.f, 0.5f, Far);
	if (ThunderSounds.Num() > 0)
	{
		int32 Index = Far < 0.35f ? 0 : FMath::RandRange(FMath::Min(1, ThunderSounds.Num() - 1), ThunderSounds.Num() - 1);
		if (ThunderSounds.Num() > 2 && Index == LastThunder && Index != 0) Index = Index == 1 ? 2 : 1;
		LastThunder = Index;
	}
	UE_LOG(LogTemp, Log, TEXT("NLWeather: lightning (thunder in %.1f s, vol %.2f)"), PendingThunderTime, PendingThunderVol);
}

void ANLWeatherDirector::UpdateLightning(float DeltaSeconds)
{
	if (FlashTime >= 0.f)
	{
		FlashTime += DeltaSeconds;
		float A = 0.f;
		for (int32 i = 0; i < FlashPulses.Num(); ++i)
		{
			const float T = FlashTime - FlashPulses[i];
			if (T >= 0.f) A = FMath::Max(A, FMath::Exp(-T / (i == FlashPulses.Num() - 1 ? 0.16f : 0.05f)) * (i == 0 ? 1.f : 0.8f));
		}
		Flash->SetIntensity(LightningIntensity * A);
		if (FlashTime > 1.5f)
		{
			FlashTime = -1.f;
			Flash->SetIntensity(0.f);
			Flash->SetVisibility(false);
		}
	}

	if (PendingThunderTime >= 0.f)
	{
		PendingThunderTime -= DeltaSeconds;
		if (PendingThunderTime < 0.f && ThunderSounds.IsValidIndex(LastThunder))
		{
			const float Vol = ThunderVolume * PendingThunderVol * FMath::Lerp(1.f, 0.6f, IndoorAlpha) * AmbVol();
			if (UAudioComponent* T = UGameplayStatics::SpawnSound2D(this, ThunderSounds[LastThunder], Vol, FMath::FRandRange(0.92f, 1.05f)))
			{
				T->SetLowPassFilterEnabled(true);
				T->SetLowPassFilterFrequency(FMath::Lerp(20000.f, 900.f, IndoorAlpha));
			}
			UE_LOG(LogTemp, Log, TEXT("NLWeather: thunder '%s' vol %.2f, indoor %.2f"), *GetNameSafe(ThunderSounds[LastThunder]), Vol, IndoorAlpha);
		}
	}

	if (!bLightning || !bRaining || Intensity < 0.9f) return;
	LightningTimer -= DeltaSeconds;
	if (LightningTimer <= 0.f)
	{
		LightningTimer = FMath::FRandRange(LightningGapMin, FMath::Max(LightningGapMin, LightningGapMax));
		const ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
		if (!Player || Player->GetMode() != ENLPlayerMode::Dead) TriggerLightning();
	}
}

void ANLWeatherDirector::UpdateAudio(float DeltaSeconds)
{
	const ANLAmbienceDirector* Amb = TActorIterator<ANLAmbienceDirector>(GetWorld()) ? *TActorIterator<ANLAmbienceDirector>(GetWorld()) : nullptr;
	IndoorAlpha = Amb ? Amb->GetIndoorAlpha() : 0.f;

	if (RainAudio)
	{
		RainAudio->SetVolumeMultiplier(FMath::Max(RainVolume * Intensity * FMath::Lerp(1.f, 0.3f, IndoorAlpha) * AmbVol(), 0.001f));
		RainAudio->SetLowPassFilterEnabled(true);
		RainAudio->SetLowPassFilterFrequency(FMath::Lerp(20000.f, 700.f, IndoorAlpha));
	}
	if (RoofAudio) RoofAudio->SetVolumeMultiplier(FMath::Max(RainRoofVolume * Intensity * IndoorAlpha * AmbVol(), 0.001f));
	if (Fog && BaseFogDensity >= 0.f) Fog->SetFogDensity(BaseFogDensity * FMath::Lerp(1.f, FogDensityMultiplier, Intensity));
}

void ANLWeatherDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bRaining ? 1.f : 0.f;
	Intensity = FMath::FInterpConstantTo(Intensity, Target, DeltaSeconds, 1.f / FMath::Max(RainFadeTime, 0.1f));

	UpdateAudio(DeltaSeconds);
	UpdateLightning(DeltaSeconds);

	const bool bShow = Intensity > 0.001f && DropData.Num() > 0;
	if (Drops->IsVisible() != bShow) Drops->SetVisibility(bShow);
	if (!bShow) return;

	if (DropMID) DropMID->SetScalarParameterValue(TEXT("Fade"), Intensity);
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	UpdateDrops(DeltaSeconds, Cam ? Cam->GetCameraLocation() : GetActorLocation());
}
