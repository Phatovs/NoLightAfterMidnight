#include "NLLighthouse.h"
#include "NLEnvKit.h"
#include "NLPowerSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

using namespace NLEnv;

namespace NLLH
{
	// Kit meshinin ölçüleri (Scripts/nl_env_kit.py)
	constexpr float MeshBaseR = 260.f, MeshTopR = 170.f, MeshH = 1000.f;
	constexpr float RoomBottom = 40.f, RoomTop = 330.f;
	constexpr float GalleryR = 232.f, LanternWall = 70.f, GlassH = 180.f;
}

ANLLighthouse::ANLLighthouse()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto SM = [this](FName Name, bool bCollide)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCollide) C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return C;
	};
	Base = SM(TEXT("Base"), true);
	Tower = SM(TEXT("Tower"), true);
	Gallery = SM(TEXT("Gallery"), true);
	LanternRoom = SM(TEXT("LanternRoom"), true);
	InteriorLantern = SM(TEXT("InteriorLantern"), false);
	NL_KIT_MESH(Base, "SM_NL_LH_Base");
	NL_KIT_MESH(Tower, "SM_NL_LH_Tower");
	NL_KIT_MESH(Gallery, "SM_NL_LH_Gallery");
	NL_KIT_MESH(LanternRoom, "SM_NL_LH_LanternRoom");
	NL_KIT_MESH(InteriorLantern, "SM_NL_Lantern");

	Boards = MakeISM(this, Root, TEXT("Boards"), true);
	Windows = MakeISM(this, Root, TEXT("Windows"), false);
	Props = MakeISM(this, Root, TEXT("Crates"), true);
	PropBarrels = MakeISM(this, Root, TEXT("Barrels"), true);
	NL_KIT_MESH(Boards, "SM_NL_Board");
	NL_KIT_MESH(Windows, "SM_NL_Pane");
	NL_KIT_MESH(Props, "SM_NL_Crate");
	NL_KIT_MESH(PropBarrels, "SM_NL_Barrel");
	// Kök Movable: bağlı parçalar da Movable olmalı, yoksa aktör taşınınca eski yerlerinde kalırlar
	for (UInstancedStaticMeshComponent* C : { Boards.Get(), Windows.Get(), Props.Get(), PropBarrels.Get() }) C->SetMobility(EComponentMobility::Movable);
	{
		static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Dark(TEXT("/Game/Environment/Materials/MI_NL_WindowDark.MI_NL_WindowDark"));
		if (Dark.Get()) Windows->SetMaterial(0, Dark.Get());
	}

	Rotor = CreateDefaultSubobject<USceneComponent>(TEXT("Rotor"));
	Rotor->SetupAttachment(Root);
	Rotor->SetMobility(EComponentMobility::Movable);
	Lens = SM(TEXT("Lens"), false);
	Lens->SetupAttachment(Rotor);
	NL_KIT_MESH(Lens, "SM_NL_LH_Lens");
	BeamA = SM(TEXT("BeamA"), false);
	BeamB = SM(TEXT("BeamB"), false);
	for (UStaticMeshComponent* B : { BeamA.Get(), BeamB.Get() })
	{
		B->SetupAttachment(Rotor);
		B->SetCastShadow(false);
		// Hareketli küçük obje: uzaklık alanı / Lumen sahnesine girmesin (her kare güncelleme maliyeti)
		B->bAffectDistanceFieldLighting = false;
		B->bAffectDynamicIndirectLighting = false;
		B->bVisibleInRayTracing = false;
		B->SetMobility(EComponentMobility::Movable);
		NL_KIT_MESH(B, "SM_NL_LH_Beam");
	}
	Lens->SetMobility(EComponentMobility::Movable);
	Lens->SetCastShadow(false);
	// Hareketli küçük obje: uzaklık alanı / Lumen sahnesine girmesin (her kare güncelleme maliyeti)
	Lens->bAffectDistanceFieldLighting = false;
	Lens->bAffectDynamicIndirectLighting = false;
	Lens->bVisibleInRayTracing = false;

	auto Spot = [this](FName Name)
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(Name);
		L->SetupAttachment(Rotor);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetCastShadows(false);
		L->SetVolumetricScatteringIntensity(4.f);
		return L;
	};
	SpotA = Spot(TEXT("SpotA"));
	SpotB = Spot(TEXT("SpotB"));

	auto Point = [this](FName Name, USceneComponent* Parent)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Parent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetCastShadows(false);
		return L;
	};
	RoomLight = Point(TEXT("RoomLight"), Rotor);
	InteriorLight = Point(TEXT("InteriorLight"), Root);
	InteriorLight->SetCastShadows(true);   // oda küçük; tek gölgeli lamba
	InteriorLight->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
}

float ANLLighthouse::RadiusAt(float Z) const
{
	using namespace NLLH;
	const float S = BaseRadius / MeshBaseR;
	const float Zl = Z / FMath::Max(TowerHeight / MeshH, 0.01f);
	return S * (MeshBaseR + (MeshTopR - MeshBaseR) * FMath::Clamp(Zl / MeshH, 0.f, 1.f));
}

void ANLLighthouse::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Layout();
}

void ANLLighthouse::Layout()
{
	using namespace NLLH;
	const float Sxy = BaseRadius / MeshBaseR;
	const float Sz = TowerHeight / MeshH;
	const float H = bBuildTower ? TowerHeight : 0.f;

	for (UStaticMeshComponent* C : { Base.Get(), Tower.Get(), Gallery.Get(), LanternRoom.Get(), InteriorLantern.Get() }) C->SetVisibility(bBuildTower);
	for (UStaticMeshComponent* C : { Base.Get(), Tower.Get(), Gallery.Get(), LanternRoom.Get() }) C->SetCollisionEnabled(bBuildTower ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	Boards->ClearInstances();
	Windows->ClearInstances();
	Props->ClearInstances();
	PropBarrels->ClearInstances();
	// Parçalar her zaman kulenin kendi ekseninde (eski kayıtlardan kalan kaymayı sıfırla)
	for (UInstancedStaticMeshComponent* C : { Boards.Get(), Windows.Get(), Props.Get(), PropBarrels.Get() })
	{
		C->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		C->SetRelativeTransform(FTransform::Identity);
	}

	const float TopR = MeshTopR * Sxy;
	Tower->SetRelativeScale3D(FVector(Sxy, Sxy, Sz));
	Base->SetRelativeScale3D(FVector(Sxy, Sxy, 0.75f));
	Gallery->SetRelativeLocation(FVector(0, 0, H));
	Gallery->SetRelativeScale3D(FVector((TopR + 55.f) / GalleryR, (TopR + 55.f) / GalleryR, 1.f));
	LanternRoom->SetRelativeLocation(FVector(0, 0, H + 14.f));
	LanternRoom->SetRelativeScale3D(FVector(Sxy, Sxy, 1.f));
	const float RotorZ = bBuildTower ? H + 14.f + LanternWall + GlassH * 0.5f : 0.f;
	Rotor->SetRelativeLocation(FVector(0, 0, RotorZ));
	Lens->SetRelativeLocation(FVector(0, 0, -45.f));

	for (int32 i = 0; i < 2; ++i)
	{
		const float Yaw = i == 0 ? 0.f : 180.f;
		const FRotator R(BeamPitch, Yaw, 0.f);
		UStaticMeshComponent* B = i == 0 ? BeamA : BeamB;
		B->SetRelativeLocationAndRotation(FVector::ZeroVector, R);
		B->SetRelativeScale3D(FVector(BeamLength / 4000.f, 1.f, 1.f));
		USpotLightComponent* L = i == 0 ? SpotA : SpotB;
		L->SetRelativeLocationAndRotation(FVector::ZeroVector, R);
		L->SetIntensity(BeamIntensity);
		L->SetAttenuationRadius(BeamRange);
		L->SetOuterConeAngle(BeamAngle);
		L->SetInnerConeAngle(BeamAngle * 0.45f);
		L->SetLightColor(BeamColor);
	}
	RoomLight->SetIntensity(15.f);
	RoomLight->SetAttenuationRadius(700.f);
	RoomLight->SetLightColor(BeamColor);

	CeilingZ = RoomTop * Sz;
	InteriorLight->SetVisibility(bBuildTower);
	if (!bBuildTower) return;

	// Zemin katı: tavandan sarkan fener + lamba
	InteriorLantern->SetRelativeLocation(FVector(0, 0, CeilingZ - 4.f));
	InteriorLight->SetRelativeLocation(FVector(0, 0, CeilingZ - 40.f));
	InteriorLight->SetIntensity(InteriorLampIntensity);
	InteriorLight->SetAttenuationRadius(900.f);

	if (bDoorPanel)
	{
	// Açık kapı (+X): kanat dışarı doğru açılmış
	const float DoorR = RadiusAt(RoomBottom * Sz + 100.f);
	const FVector Hinge(DoorR + 2.f, 60.f, RoomBottom * Sz);
	const float Open = FMath::DegreesToRadians(105.f);
	const FVector D(FMath::Sin(Open), -FMath::Cos(Open), 0.f);
	const FVector N(-D.Y, D.X, 0.f);
	for (int32 k = 0; k < 5; ++k)
	{
		Add(Boards, Hinge + D * (23.f * (k + 0.5f)) + FVector(0, 0, 106.f), Orient(FVector::UpVector, N), FVector(2.1f, 22.f / 20.f, 1.f));
	}
	for (float Hz : { 30.f, 180.f })
	{
		Add(Boards, Hinge + D * 57.f + N * 3.f + FVector(0, 0, Hz), Orient(D, N), FVector(1.1f, 0.7f, 0.8f));
	}
	// Kasa
	for (float Y : { -64.f, 64.f })
	{
		Add(Boards, FVector(DoorR + 3.f, Y, RoomBottom * Sz + 108.f), Orient(FVector::UpVector, FVector(0, 1, 0)), FVector(2.2f, 0.7f, 2.f));
	}
	Add(Boards, FVector(DoorR + 3.f, 0.f, RoomBottom * Sz + 220.f), Orient(FVector(0, 1, 0), FVector::UpVector), FVector(1.4f, 0.7f, 2.f));
	// Kapı önü basamağı
	Add(Boards, FVector(DoorR + 40.f, 0.f, 18.f), Orient(FVector(0, 1, 0), FVector::UpVector), FVector(1.5f, 2.5f, 1.5f));
	}

	// Küçük pencereler (kapıya denk gelmeyen yönlerde)
	const float WinZ[] = { 0.42f, 0.62f, 0.8f };
	const float WinA[] = { 200.f, 320.f, 80.f };
	for (int32 i = 0; i < 3; ++i)
	{
		const float Z = TowerHeight * WinZ[i];
		const float A = FMath::DegreesToRadians(WinA[i]);
		const FVector Out(FMath::Cos(A), FMath::Sin(A), 0.f);
		const FVector Tan(-Out.Y, Out.X, 0.f);
		const float R = RadiusAt(Z);
		Add(Windows, Out * (R + 1.f) + FVector(0, 0, Z), Orient(Tan, FVector::UpVector), FVector(0.42f, 2.f, 0.75f));
		Add(Boards, Out * (R + 4.f) + FVector(0, 0, Z - 42.f), Orient(Tan, FVector::UpVector), FVector(0.6f, 0.6f, 1.5f));
	}

	if (bInteriorProps)
	{
		const float Floor = RoomBottom * Sz;
		Add(Props, FVector(-110.f, 70.f, Floor), FQuat(FVector::UpVector, 0.3f), FVector::OneVector);
		Add(Props, FVector(-115.f, 75.f, Floor + 70.f), FQuat(FVector::UpVector, 0.9f), FVector(0.8f));
		Add(PropBarrels, FVector(-60.f, -130.f, Floor), FQuat::Identity, FVector::OneVector);
		Add(PropBarrels, FVector(10.f, -150.f, Floor), FQuat(FVector::UpVector, 1.f), FVector::OneVector);
	}
}

void ANLLighthouse::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* B : { BeamA.Get(), BeamB.Get() })
	{
		if (UMaterialInstanceDynamic* M = B->CreateAndSetMaterialInstanceDynamic(0))
		{
			M->SetVectorParameterValue(TEXT("Color"), BeamColor);
			BeamMIDs.Add(M);
		}
	}
	LensMID = Lens->CreateAndSetMaterialInstanceDynamic(0);
	Stutter.Interval = FVector2D(14.f, 35.f);
	ApplyLit(1.f);
}

void ANLLighthouse::ApplyLit(float InAlpha)
{
	LitAlpha = InAlpha;
	const float Alpha = InAlpha * StutterMul;
	SpotA->SetIntensity(BeamIntensity * Alpha);
	SpotB->SetIntensity(BeamIntensity * Alpha);
	RoomLight->SetIntensity(15.f * Alpha);
	if (bBuildTower) InteriorLight->SetIntensity(InteriorLampIntensity * Alpha);
	for (UMaterialInstanceDynamic* M : BeamMIDs) if (M) M->SetScalarParameterValue(TEXT("Intensity"), BeamGlow * Alpha);
	if (LensMID) LensMID->SetScalarParameterValue(TEXT("Intensity"), 2.5f * Alpha + 0.02f);
	const bool bOn = Alpha > 0.001f;
	SpotA->SetVisibility(bOn);
	SpotB->SetVisibility(bOn && !bManualAim);
	BeamA->SetVisibility(bOn);
	BeamB->SetVisibility(bOn && !bManualAim);
}

void ANLLighthouse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bManualAim)
	{
		const FVector From = Rotor->GetComponentLocation();
		const FRotator Want = (AimPoint - From).Rotation();
		Rotor->SetWorldRotation(FMath::RInterpTo(Rotor->GetComponentRotation(), Want, DeltaSeconds, AimSpeed));
		// Görünen huzme hedefin biraz ötesinde söner
		const float Len = FMath::Clamp(FVector::Dist(From, AimPoint) * 1.15f, 800.f, 20000.f);
		BeamA->SetRelativeScale3D(FVector(Len / 4000.f, 1.f, 1.f));
	}
	else
	{
		Rotor->AddLocalRotation(FRotator(0.f, 360.f / FMath::Max(RotationPeriod, 1.f) * DeltaSeconds, 0.f));
	}

	bool bWant = true;
	if (bRequirePower)
	{
		if (const UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
		{
			if (Power->GetGenerator()) bWant = Power->HasPower();
		}
	}
	// Tekleme: lamba yanıyorken ara ara kırpar
	const float M = LitAlpha > 0.5f ? Stutter.Update(GetWorld()->GetTimeSeconds()) : 1.f;
	if (!FMath::IsNearlyEqual(M, StutterMul))
	{
		StutterMul = M;
		ApplyLit(LitAlpha);
	}

	const float Target = bWant ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(LitAlpha, Target))
	{
		// Sönerken hızlı, yanarken lamba ısınır gibi yavaş
		const float Speed = bWant ? 0.6f : 2.5f;
		ApplyLit(FMath::FInterpConstantTo(LitAlpha, Target, DeltaSeconds, Speed));
	}
}

void ANLLighthouse::AimAt(const FVector& WorldPoint, float Speed)
{
	if (!bManualAim)
	{
		bManualAim = true;
		// Tek huzme: arkaya bakan ikincisi menüde gerekmez
		BeamB->SetVisibility(false);
		SpotB->SetVisibility(false);
		BeamA->SetRelativeRotation(FRotator::ZeroRotator);
		SpotA->SetRelativeRotation(FRotator::ZeroRotator);
	}
	AimPoint = WorldPoint;
	AimSpeed = Speed;
}
