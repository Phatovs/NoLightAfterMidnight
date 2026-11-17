#include "NLPlankDock.h"
#include "NLEnvKit.h"
#include "Components/PointLightComponent.h"

using namespace NLEnv;

ANLPlankDock::ANLPlankDock()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Static);

	// Döşeme görsel; yürünen yüzey level'deki (gizli) rıhtım kutusunun çarpışması olabilir, ama kendi çarpışması da var
	Deck = MakeISM(this, Root, TEXT("Deck"), true);
	Posts = MakeISM(this, Root, TEXT("Posts"), true);
	Logs = MakeISM(this, Root, TEXT("Logs"), false);
	Ropes = MakeISM(this, Root, TEXT("Ropes"), false);
	LanternPosts = MakeISM(this, Root, TEXT("LanternPosts"), true);
	Lanterns = MakeISM(this, Root, TEXT("Lanterns"), false);
	NL_KIT_MESH(Deck, "SM_NL_DeckPanel");
	NL_KIT_MESH(Posts, "SM_NL_Post");
	NL_KIT_MESH(Logs, "SM_NL_Log");
	NL_KIT_MESH(Ropes, "SM_NL_RopeCoil");
	NL_KIT_MESH(LanternPosts, "SM_NL_LanternPost");
	NL_KIT_MESH(Lanterns, "SM_NL_GuideLantern");
	Ropes->SetCullDistances(0, 5000);
	Lanterns->SetCullDistances(0, 8000);

	auto MakeLight = [this](FName Name)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetCastShadows(false);
		return L;
	};
	LightA = MakeLight(TEXT("LanternLightA"));
	LightB = MakeLight(TEXT("LanternLightB"));
}

void ANLPlankDock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANLPlankDock::Rebuild()
{
	for (UInstancedStaticMeshComponent* C : { Deck.Get(), Posts.Get(), Logs.Get(), Ropes.Get(), LanternPosts.Get(), Lanterns.Get() }) C->ClearInstances();
	FRandomStream Rng(Seed);
	const float HL = Length * 0.5f, HW = Width * 0.5f;

	// Döşeme: 200 x 100 paneller, tahtalar iskele boyunca değil enine (X'e dik) olsun diye panel 90° döner
	for (float X = -HL; X < HL - 1.f; X += 100.f)
	{
		const float X1 = FMath::Min(X + 100.f, HL);
		for (float Y = -HW; Y < HW - 1.f; Y += 200.f)
		{
			const float Y1 = FMath::Min(Y + 200.f, HW);
			const FQuat Rot = FQuat(FVector::UpVector, HALF_PI) * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Rng.FRandRange(-0.6f, 0.6f)));
			Add(Deck, FVector((X + X1) * 0.5f, (Y + Y1) * 0.5f, Rng.FRandRange(-0.6f, 0.4f)), Rot, FVector((Y1 - Y) / 200.f, (X1 - X) / 100.f, 1.f));
		}
	}
	// Kenar kirişleri (kütük) döşemenin altında
	for (int32 s = -1; s <= 1; s += 2)
	{
		Add(Logs, FVector(0, s * (HW - 18.f), -18.f), FQuat::Identity, FVector(Length / 100.f, 1.1f, 1.1f));
	}
	Add(Logs, FVector(0, 0, -20.f), FQuat::Identity, FVector(Length / 100.f, 1.f, 1.f));

	// Kazıklar ve babalar
	int32 n = 0;
	for (float X = -HL + 40.f; X <= HL - 20.f; X += PostSpacing, ++n)
	{
		for (int32 s = -1; s <= 1; s += 2)
		{
			const bool bBollard = BollardEvery > 0 && (n % BollardEvery == 0) && X > -HL + 200.f;
			const float Top = bBollard ? 75.f : -6.f;
			const float H = Top + PostDepth;
			const FVector Base(X + Rng.FRandRange(-8.f, 8.f), s * (HW + 6.f), -PostDepth);
			const FQuat Tilt(FVector(Rng.FRandRange(-1.f, 1.f), Rng.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal(), FMath::DegreesToRadians(Rng.FRandRange(0.f, 2.5f)));
			Add(Posts, Base, Tilt, FVector(1.15f, 1.15f, H / 100.f));
			if (bBollard && Rng.FRand() < 0.7f)
			{
				// Babaya sarılı halat (ve bazen yere bırakılmış bir kangal)
				Add(Ropes, FVector(Base.X, Base.Y, 20.f), FQuat(FVector::UpVector, Rng.FRandRange(0.f, 6.28f)), FVector(0.55f, 0.55f, 0.8f));
				if (Rng.FRand() < 0.5f) Add(Ropes, FVector(Base.X + Rng.FRandRange(-60.f, 60.f), s * (HW - 70.f), 0.f), FQuat(FVector::UpVector, Rng.FRandRange(0.f, 6.28f)), FVector::OneVector);
			}
		}
	}

	// Uçta fenerli direkler
	LightA->SetVisibility(false);
	LightB->SetVisibility(false);
	if (bEndLanterns)
	{
		int32 i = 0;
		for (int32 s = -1; s <= 1; s += 2, ++i)
		{
			const FVector Base(HL - 60.f, s * (HW - 22.f), 0.f);
			// Kol iskelenin ortasına doğru uzanır, fener oraya asılı
			const FVector ArmDir(0.f, -s, 0.f);
			Add(LanternPosts, Base, Orient(ArmDir, FVector::UpVector), FVector::OneVector);
			const FVector LanternAt = Base + ArmDir * 78.f + FVector(0, 0, 292.f);
			Add(Lanterns, LanternAt, Orient(FVector::XAxisVector, FVector::UpVector), FVector::OneVector);
			UPointLightComponent* L = i == 0 ? LightA : LightB;
			L->SetRelativeLocation(LanternAt - FVector(0, 0, 24.f));
			L->SetIntensity(LanternIntensity);
			L->SetAttenuationRadius(LanternRadius);
			L->SetLightColor(LampColor);
			L->SetVisibility(true);
		}
	}
}
