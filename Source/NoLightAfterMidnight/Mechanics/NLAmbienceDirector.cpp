#include "NLAmbienceDirector.h"
#include "NLDefaults.h"
#include "NLGameMode.h"
#include "NLPlayerCharacter.h"
#include "NLRunner.h"
#include "NLGameSubsystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ANLAmbienceDirector::ANLAmbienceDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(GustLoop, "amb_wind_gust_loop");
	NL_DEFAULT_SOUND(WavesLoop, "amb_waves_loop");
	NL_DEFAULT_SOUND(InteriorLoop, "amb_interior_hum_loop");
	NL_DEFAULT_SOUND(GlassWindLoop, "amb_glass_wind_loop");
	NL_DEFAULT_SOUND(GlassGustHit, "glass_gust_hit");
	// Yol sabit metin olmalı (yükleyici metni sonradan okur)
	static const TCHAR* DistantPaths[] = {
		TEXT("/Game/Audio/NL/dist_branch_snap.dist_branch_snap"), TEXT("/Game/Audio/NL/dist_metal_clank.dist_metal_clank"),
		TEXT("/Game/Audio/NL/dist_footsteps.dist_footsteps"), TEXT("/Game/Audio/NL/dist_whisper.dist_whisper"),
		TEXT("/Game/Audio/NL/dist_moan.dist_moan"), TEXT("/Game/Audio/NL/dist_tree_creak.dist_tree_creak"),
		TEXT("/Game/Audio/NL/dist_foghorn.dist_foghorn"), TEXT("/Game/Audio/NL/dist_knock.dist_knock") };
	for (const TCHAR* Path : DistantPaths)
	{
		ConstructorHelpers::FObjectFinderOptional<USoundBase> Finder(Path);
		if (Finder.Get()) DistantSounds.Add(Finder.Get());
	}
}

void ANLAmbienceDirector::BeginPlay()
{
	Super::BeginPlay();

	// Uzaktan sesler: 3B, uzaklaştıkça kısılır ve boğuklaşır
	DistantAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = DistantAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(600.f, 0.f, 0.f);
		A.FalloffDistance = 6000.f;
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = 800.f;
		A.LPFRadiusMax = 5000.f;
		A.LPFFrequencyAtMax = 2500.f;
	}
	WavesAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = WavesAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
		A.FalloffDistance = 3500.f;
	}

	// Camlara çarpan rüzgâr: duvardan gelir, yakında net duyulur
	GlassAttenuation = NewObject<USoundAttenuation>(this);
	{
		FSoundAttenuationSettings& A = GlassAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
		A.FalloffDistance = 1500.f;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) { if (It->ActorHasTag(GlassTag)) GlassActors.Add(*It); }
	GlassAudio = UGameplayStatics::SpawnSound2D(this, GlassWindLoop, 0.f, 1.f, 0.f, nullptr, true, false);
	if (GlassAudio) GlassAudio->SetVolumeMultiplier(0.001f);

	GustAudio = UGameplayStatics::SpawnSound2D(this, GustLoop, GustVolume, 1.f, 0.f, nullptr, true, false);
	InteriorAudio = UGameplayStatics::SpawnSound2D(this, InteriorLoop, 0.f, 1.f, 0.f, nullptr, true, false);
	if (InteriorAudio) InteriorAudio->SetVolumeMultiplier(0.001f);

	DistantTimer = FirstDistantDelay;
	// Kıyı ışınları için zemin/collision hazır olsun diye bir kare sonra
	GetWorldTimerManager().SetTimerForNextTick(this, &ANLAmbienceDirector::SetupShore);
}

void ANLAmbienceDirector::SetupShore()
{
	if (!WavesLoop) return;
	// Merkez: oyuncunun başladığı yer değil, kıyı aramak için adanın ortası olarak bu aktörün konumu (GameMode dünya merkezine koyar)
	const FVector Center = GetActorLocation();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLShore), false);
	int32 Found = 0;
	for (int32 r = 0; r < ShoreRays; ++r)
	{
		const float Ang = 2.f * PI * r / ShoreRays;
		const FVector Dir(FMath::Cos(Ang), FMath::Sin(Ang), 0.f);
		FVector LastLand = FVector::ZeroVector;
		bool bHadLand = false;
		for (float D = 300.f; D <= ShoreSearchRadius; D += 200.f)
		{
			const FVector P = Center + Dir * D;
			FHitResult Hit;
			const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, 5000.f), FVector(P.X, P.Y, -3000.f), ECC_Visibility, Q);
			const float Z = bHit ? Hit.ImpactPoint.Z : -10000.f;
			if (Z > SeaLevelZ) { LastLand = FVector(P.X, P.Y, Z); bHadLand = true; }
			else if (bHadLand)
			{
				// Kıyı: son kara noktası
				if (UAudioComponent* W = UGameplayStatics::SpawnSoundAtLocation(this, WavesLoop, LastLand + FVector(0, 0, 50.f), FRotator::ZeroRotator,
					WavesVolume, FMath::FRandRange(0.92f, 1.06f), FMath::FRandRange(0.f, 12.f), WavesAttenuation, nullptr, false))
				{
					WaveAudios.Add(W);
					++Found;
				}
				break;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("NLAmbience: %d shore wave sources, %d distant sounds"), Found, DistantSounds.Num());
}

bool ANLAmbienceDirector::IsQuietMoment() const
{
	// Kovalama, kapı tutma, ölüm: tekil sesler susar (ana tehdit dikkat dağıtmasın)
	const ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player) return true;
	const ENLPlayerMode M = Player->GetMode();
	if (M == ENLPlayerMode::Dead || M == ENLPlayerMode::HoldingDoor) return true;
	for (TActorIterator<ANLRunner> It(GetWorld()); It; ++It)
	{
		if (It->IsChasing()) return true;
	}
	return false;
}

void ANLAmbienceDirector::PlayDistantSound()
{
	ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || DistantSounds.Num() == 0) return;

	int32 Index = FMath::RandRange(0, DistantSounds.Num() - 1);
	if (DistantSounds.Num() > 1 && Index == LastDistantIndex) Index = (Index + 1) % DistantSounds.Num();
	LastDistantIndex = Index;

	// Oyuncunun arkasından ya da yanından (görüş alanının dışından)
	const FRotator View = Player->GetControlRotation();
	const float Yaw = View.Yaw + 180.f + FMath::FRandRange(-110.f, 110.f);
	const float Dist = FMath::FRandRange(DistantMinDistance, DistantMaxDistance);
	FVector P = Player->GetActorLocation() + FRotator(0.f, Yaw, 0.f).Vector() * Dist;
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLDistant), false, Player);
	if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, P.Z + 2000.f), FVector(P.X, P.Y, P.Z - 3000.f), ECC_Visibility, Q)) P.Z = Hit.ImpactPoint.Z + 80.f;

	const UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this);
	const float Vol = DistantVolume * FMath::FRandRange(0.6f, 1.f) * FMath::Lerp(1.f, 0.55f, IndoorAlpha) * (Sub ? Sub->GetAmbienceVolume() : 1.f);
	UGameplayStatics::PlaySoundAtLocation(this, DistantSounds[Index], P, FRotator::ZeroRotator, Vol, FMath::FRandRange(0.9f, 1.08f), 0.f, DistantAttenuation);
	UE_LOG(LogTemp, Log, TEXT("NLAmbience: distant '%s' %.0f cm away (%.0f deg from view), vol %.2f, indoor %.2f"),
		*GetNameSafe(DistantSounds[Index]), Dist, FMath::FindDeltaAngleDegrees(View.Yaw, Yaw), Vol, IndoorAlpha);
}

void ANLAmbienceDirector::DebugBurst(int32 Count, float Gap)
{
	for (int32 i = 0; i < Count; ++i)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]() { PlayDistantSound(); }), 0.1f + i * Gap, false);
	}
}

void ANLAmbienceDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ANLPlayerCharacter* Player = Cast<ANLPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player) return;

	// İçeride mi: başının üstünde tavan var mı (4 kez/sn)
	IndoorCheckTimer -= DeltaSeconds;
	if (IndoorCheckTimer <= 0.f)
	{
		IndoorCheckTimer = 0.25f;
		const FVector Head = Player->GetActorLocation() + FVector(0.f, 0.f, 70.f);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(NLRoof), false, Player);
		const bool bNow = GetWorld()->LineTraceSingleByChannel(Hit, Head, Head + FVector(0.f, 0.f, RoofCheckHeight), ECC_Visibility, Q);
		if (bNow != bIndoor)
		{
			bIndoor = bNow;
			UE_LOG(LogTemp, Log, TEXT("NLAmbience: player %s"), bIndoor ? TEXT("went indoors") : TEXT("went outdoors"));
		}
	}
	IndoorAlpha = FMath::FInterpTo(IndoorAlpha, bIndoor ? 1.f : 0.f, DeltaSeconds, 2.5f);
	ApplyMix();
	UpdateGlass(DeltaSeconds, Player);

	// Uzaktan tekil sesler
	DistantTimer -= DeltaSeconds;
	if (DistantTimer <= 0.f)
	{
		if (IsQuietMoment())
		{
			DistantTimer = 5.f;   // sakinleşince tekrar dene
		}
		else
		{
			PlayDistantSound();
			DistantTimer = FMath::FRandRange(DistantGapMin, FMath::Max(DistantGapMin, DistantGapMax));
		}
	}
}

void ANLAmbienceDirector::ApplyMix()
{
	const UNLGameSubsystem* Sub = UNLGameSubsystem::Get(this);
	const float Amb = Sub ? Sub->GetAmbienceVolume() : 1.f;   // ayarlar: ortam sesi
	const float OutsideVol = FMath::Lerp(1.f, IndoorOutsideVolume, IndoorAlpha) * Amb;
	const float Lpf = FMath::Lerp(20000.f, IndoorLowPassHz, IndoorAlpha);

	// Rüzgâr şiddeti yavaşça dalgalanır (dosyadaki kabarmaların üstüne)
	GustPhase += GetWorld()->GetDeltaSeconds();
	const float Swell = 0.75f + 0.25f * FMath::Sin(GustPhase * 0.11f) * FMath::Sin(GustPhase * 0.037f + 1.3f);

	auto Mix = [OutsideVol, Lpf](UAudioComponent* C, float Base)
	{
		if (!C) return;
		C->SetVolumeMultiplier(FMath::Max(Base * OutsideVol, 0.001f));
		C->SetLowPassFilterEnabled(true);
		C->SetLowPassFilterFrequency(Lpf);
	};
	Mix(GustAudio, GustVolume * Swell);
	for (UAudioComponent* W : WaveAudios) Mix(W, WavesVolume);
	if (const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>())
	{
		Mix(GM->GetAmbientAudio(), 1.f);
	}
	if (InteriorAudio) InteriorAudio->SetVolumeMultiplier(FMath::Max(InteriorVolume * IndoorAlpha * Amb, 0.001f));
}

void ANLAmbienceDirector::UpdateGlass(float DeltaSeconds, const ANLPlayerCharacter* Player)
{
	// Camlı binanın (deniz feneri) içinde mi: içeride ve binanın gövdesinin içinde (yatayda)
	const AActor* Inside = nullptr;
	if (bIndoor)
	{
		const FVector P = Player->GetActorLocation();
		for (const AActor* G : GlassActors)
		{
			if (!G) continue;
			FVector Origin, Extent;
			G->GetActorBounds(true, Origin, Extent);
			if (FVector::Dist2D(P, Origin) < FMath::Min(Extent.X, Extent.Y) + 30.f && P.Z > Origin.Z - Extent.Z && P.Z < Origin.Z + Extent.Z)
			{
				Inside = G;
				break;
			}
		}
	}
	if (Inside != CurrentGlass)
	{
		UE_LOG(LogTemp, Log, TEXT("NLAmbience: %s"), Inside ? TEXT("inside the lighthouse: wind on the glass") : TEXT("left the lighthouse"));
		CurrentGlass = Inside;
	}
	GlassAlpha = FMath::FInterpTo(GlassAlpha, Inside ? 1.f : 0.f, DeltaSeconds, 2.f);
	if (GlassAudio) GlassAudio->SetVolumeMultiplier(FMath::Max(GlassWindVolume * GlassAlpha * (UNLGameSubsystem::Get(this) ? UNLGameSubsystem::Get(this)->GetAmbienceVolume() : 1.f), 0.001f));

	if (!Inside || !GlassGustHit) return;
	GlassHitTimer -= DeltaSeconds;
	if (GlassHitTimer > 0.f) return;
	GlassHitTimer = FMath::FRandRange(GlassHitGapMin, FMath::Max(GlassHitGapMin, GlassHitGapMax));

	// Rastgele bir camdan: binanın duvarında, oyuncunun yüksekliğinde
	FVector Origin, Extent;
	Inside->GetActorBounds(true, Origin, Extent);
	const float Ang = FMath::FRandRange(0.f, 2.f * PI);
	const float R = FMath::Min(Extent.X, Extent.Y) * 0.85f;
	const FVector At(Origin.X + FMath::Cos(Ang) * R, Origin.Y + FMath::Sin(Ang) * R, Player->GetActorLocation().Z + FMath::FRandRange(20.f, 120.f));
	const float Vol = GlassHitVolume * FMath::FRandRange(0.6f, 1.f);
	UGameplayStatics::PlaySoundAtLocation(this, GlassGustHit, At, FRotator::ZeroRotator, Vol, FMath::FRandRange(0.85f, 1.1f), 0.f, GlassAttenuation);
	UE_LOG(LogTemp, Log, TEXT("NLAmbience: gust hits the glass %.0f cm away, vol %.2f"), FVector::Dist(At, Player->GetActorLocation()), Vol);
}
