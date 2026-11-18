#include "NLHeater.h"
#include "NLDefaults.h"
#include "NLPowerConsumerComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"

TArray<TWeakObjectPtr<ANLHeater>> ANLHeater::AllHeaters;

ANLHeater::ANLHeater()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	{
		static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> M(TEXT("/Game/Environment/Kit/SM_NL_Heater.SM_NL_Heater"));
		Mesh->SetStaticMesh(M.Get());
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->bVisibleInRayTracing = false;

	// Çubukların önünde, yere ve karşı duvara turuncu ışık
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetRelativeLocation(FVector(26.f, 0.f, 38.f));
	Glow->SetAttenuationRadius(420.f);
	Glow->SetSourceRadius(22.f);
	Glow->SetSoftSourceRadius(10.f);
	Glow->SetCastShadows(true);
	Glow->bAffectsWorld = true;
	Glow->SetIntensity(0.f);

	Hum = CreateDefaultSubobject<UAudioComponent>(TEXT("Hum"));
	Hum->SetupAttachment(Mesh);
	Hum->bAutoActivate = false;
	NL_DEFAULT_SOUND(HumLoop, "amb_interior_hum_loop");

	PowerConsumer = CreateDefaultSubobject<UNLPowerConsumerComponent>(TEXT("PowerConsumer"));
}

void ANLHeater::BeginPlay()
{
	Super::BeginPlay();
	AllHeaters.RemoveAll([](const TWeakObjectPtr<ANLHeater>& H) { return !H.IsValid(); });
	AllHeaters.AddUnique(this);
	const int32 Slot = Mesh->GetMaterialIndex(TEXT("MI_NL_HeaterGlow"));
	if (Slot != INDEX_NONE) GlowMID = Mesh->CreateDynamicMaterialInstance(Slot);
	Glow->SetLightColor(LightColor);
	if (HumLoop) Hum->SetSound(HumLoop);
	// Oyun başında elektrik varsa hemen sıcak başlasın
	HeatAlpha = PowerConsumer->IsPowered() ? 1.f : 0.f;
	Apply();
}

void ANLHeater::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AllHeaters.Remove(this);
	Super::EndPlay(EndPlayReason);
}

void ANLHeater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bOn = PowerConsumer->IsPowered();
	const float Rate = 1.f / FMath::Max(bOn ? WarmUpTime : CoolDownTime, 0.1f);
	const float Prev = HeatAlpha;
	HeatAlpha = FMath::FInterpConstantTo(HeatAlpha, bOn ? 1.f : 0.f, DeltaSeconds, Rate);
	FlickerTime += DeltaSeconds;
	if (HeatAlpha > 0.001f || Prev > 0.001f) Apply();
}

void ANLHeater::Apply()
{
	// Akkor tel: önce koyu kırmızı, ısındıkça turuncu; hafif dalgalanır
	const float Heat = FMath::SmoothStep(0.f, 1.f, HeatAlpha);
	const float Wobble = 1.f + 0.035f * FMath::Sin(FlickerTime * 7.3f) + 0.02f * FMath::Sin(FlickerTime * 17.1f + 1.3f);
	if (GlowMID)
	{
		GlowMID->SetScalarParameterValue(TEXT("Intensity"), GlowIntensity * FMath::Pow(Heat, 1.6f) * Wobble);
		const FLinearColor Col = FMath::Lerp(FLinearColor(0.6f, 0.06f, 0.01f), FLinearColor(1.f, 0.32f, 0.06f), Heat);
		GlowMID->SetVectorParameterValue(TEXT("Color"), Col);
	}
	Glow->SetIntensity(LightIntensity * FMath::Pow(Heat, 1.4f) * Wobble);
	Glow->SetVisibility(Heat > 0.01f);
	if (HumLoop)
	{
		const float Vol = HumVolume * HeatAlpha;
		if (Vol > 0.005f)
		{
			if (!Hum->IsPlaying()) Hum->Play(FMath::FRandRange(0.f, 2.f));
			Hum->SetVolumeMultiplier(Vol);
			Hum->SetPitchMultiplier(1.35f);
		}
		else if (Hum->IsPlaying())
		{
			Hum->Stop();
		}
	}
}

bool ANLHeater::WarmsLocation(const FVector& Location) const
{
	if (!IsRunning()) return false;
	const FVector Front = GetActorLocation() + GetActorForwardVector() * 20.f + FVector(0.f, 0.f, 40.f);
	if (FVector::DistSquared(Front, Location) > FMath::Square(WarmRadius)) return false;
	if (FMath::Abs(Location.Z - Front.Z) > 200.f) return false;   // üst/alt kat değil
	// Arada duvar var mı (göğüs hizasına: masa vb. alçak eşyalar engellemez)
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(NLHeaterLOS), false, this);
	const FVector From = Front + FVector(0.f, 0.f, 25.f);
	const FVector To = Location + FVector(0.f, 0.f, 40.f);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Q)) return true;
	return Hit.GetActor() && Hit.GetActor()->IsA<APawn>();
}

bool ANLHeater::IsWarmAt(const UWorld* World, const FVector& Location)
{
	for (const TWeakObjectPtr<ANLHeater>& H : AllHeaters)
	{
		if (H.IsValid() && H->GetWorld() == World && H->WarmsLocation(Location)) return true;
	}
	return false;
}
