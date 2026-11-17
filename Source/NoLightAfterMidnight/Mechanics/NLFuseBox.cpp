#include "NLFuseBox.h"
#include "NLDefaults.h"
#include "NLPowerSubsystem.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "NLSaboteur.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"

namespace NLFuseLayout
{
	// SM_NL_FuseCabinet yerel koordinatları (Scripts/blender_fusebox.py ile aynı): arka yüz duvarda (Y=0), ön +Y.
	// Kabin X -33..33, Z -12..80. Yuva 0-2 üst sıra dikey (CAM 1-3), yuva 3 yatay (FLASH).
	static const FVector SlotPos[ANLFuseBox::NumFuses] =
	{
		FVector(-18.f, 6.5f, 62.f), FVector(0.f, 6.5f, 62.f), FVector(18.f, 6.5f, 62.f),
		FVector(16.f, 6.5f, 23.f)
	};
	static const bool SlotHorizontal[ANLFuseBox::NumFuses] = { false, false, false, true };
	// Sigortaların ön yüzü (fare ışını bu düzleme vurur)
	static constexpr float PanelY = 8.f;
	static const FVector LedPos[ANLFuseBox::NumFuses] =
	{
		FVector(-18.f, 4.6f, 75.5f), FVector(0.f, 4.6f, 75.5f), FVector(18.f, 4.6f, 75.5f), FVector(30.5f, 4.6f, 27.f)
	};
	// İbre mafsalları (kadranın önünde) ve boyları: 0-2 CAM, 3 FLASH, 4 ana gösterge
	static constexpr int32 NumGauges = 5;
	static const FVector NeedlePos[NumGauges] =
	{
		FVector(-18.f, 6.f, 40.2f), FVector(0.f, 6.f, 40.2f), FVector(18.f, 6.f, 40.2f), FVector(20.f, 6.f, 7.35f), FVector(-14.f, 6.f, 1.f)
	};
	static const float NeedleScale[NumGauges] = { 1.f, 1.f, 1.f, 0.95f, 1.7f };
}

ANLFuseBox::ANLFuseBox()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(SwitchOnSound, "fuse_switch_on");
	NL_DEFAULT_SOUND(SwitchOffSound, "fuse_switch_off");
	NL_DEFAULT_SOUND(TripSound, "fuse_trip");
	NL_DEFAULT_SOUND(FuseMoveSound, "charger_dock");

	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CabinetFinder(TEXT("/Game/Environment/Kit/SM_NL_FuseCabinet.SM_NL_FuseCabinet"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CartridgeFinder(TEXT("/Game/Environment/Kit/SM_NL_FuseCartridge.SM_NL_FuseCartridge"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> LeverFinder(TEXT("/Game/Environment/Kit/SM_NL_FuseLever.SM_NL_FuseLever"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> NeedleFinder(TEXT("/Game/Environment/Kit/SM_NL_GaugeNeedle.SM_NL_GaugeNeedle"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> LedFinder(TEXT("/Game/Environment/Kit/SM_NL_FuseLED.SM_NL_FuseLED"));
	CabinetAsset = CabinetFinder.Get();
	LeverAsset = LeverFinder.Get();

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	RootComponent = BoxMesh;
	BoxMesh->SetStaticMesh(CabinetAsset);

	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	LeverMesh->SetupAttachment(BoxMesh);
	LeverMesh->SetStaticMesh(LeverAsset);
	LeverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Yakın plan kamera: kabinin tamamı kadrajda
	FuseViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FuseViewCamera"));
	FuseViewCamera->SetupAttachment(BoxMesh);
	FuseViewCamera->SetRelativeLocationAndRotation(FVector(0.f, 182.f, 36.f), FRotator(-2.f, -90.f, 0.f));
	FuseViewCamera->SetFieldOfView(60.f);
	FuseViewCamera->bAutoActivate = true;

	ViewLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FuseViewLight"));
	ViewLight->SetupAttachment(BoxMesh);
	ViewLight->SetRelativeLocation(FVector(10.f, 70.f, 95.f));
	ViewLight->SetIntensity(0.f);
	ViewLight->SetAttenuationRadius(260.f);
	ViewLight->SetLightColor(FLinearColor(0.85f, 0.9f, 1.f));
	ViewLight->SetCastShadows(false);

	auto Part = [this](const FString& Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		C->SetupAttachment(BoxMesh);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		// Küçük hareketli parçalar: uzaklık alanı / Lumen / ray tracing sahnesine girmesin
		C->bAffectDistanceFieldLighting = false;
		C->bAffectDynamicIndirectLighting = false;
		C->bVisibleInRayTracing = false;
		return C;
	};
	for (int32 i = 0; i < NumFuses; ++i)
	{
		FuseBodies.Add(Part(FString::Printf(TEXT("FuseBody%d"), i), CartridgeFinder.Get()));
		Leds.Add(Part(FString::Printf(TEXT("FuseLed%d"), i), LedFinder.Get()));
	}
	for (int32 i = 0; i < NLFuseLayout::NumGauges; ++i)
	{
		Needles.Add(Part(FString::Printf(TEXT("GaugeNeedle%d"), i), NeedleFinder.Get()));
	}
	ApplyModel();
}

void ANLFuseBox::ApplyModel()
{
	using namespace NLFuseLayout;
	// Leveldeki eski kurulum (küp kutu, küp kol) kaydedilmiş olabilir: modeli ve yerleşimi her zaman buradan uygula
	if (CabinetAsset && BoxMesh->GetStaticMesh() != CabinetAsset) BoxMesh->SetStaticMesh(CabinetAsset);
	if (LeverAsset && LeverMesh->GetStaticMesh() != LeverAsset) LeverMesh->SetStaticMesh(LeverAsset);
	LeverMesh->SetRelativeScale3D(FVector::OneVector);
	LeverPivot = FVector(5.f, 7.f, 6.f);
	LeverOutward = FVector(0.f, 1.f, 0.f);
	LeverLength = 14.f;
	LeverOnAngle = 55.f;
	LeverOffAngle = -55.f;
	FuseViewCamera->SetRelativeLocationAndRotation(FVector(0.f, 182.f, 36.f), FRotator(-2.f, -90.f, 0.f));
	FuseViewCamera->SetFieldOfView(60.f);
	ViewLight->SetRelativeLocation(FVector(10.f, 70.f, 95.f));
	for (int32 i = 0; i < Leds.Num(); ++i) if (Leds[i]) Leds[i]->SetRelativeLocation(LedPos[i]);
	for (int32 i = 0; i < Needles.Num(); ++i)
	{
		if (!Needles[i]) continue;
		Needles[i]->SetRelativeLocation(NeedlePos[i]);
		Needles[i]->SetRelativeScale3D(FVector(NeedleScale[i]));
		Needles[i]->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(-58.f)));
	}
	for (int32 i = 0; i < FuseBodies.Num(); ++i) PlaceFuseMeshes(i, SlotPos[i], 1.f, SlotHorizontal[i]);
	PlaceLever(LeverAngle);
}

void ANLFuseBox::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LeverAngle = bStartOn ? LeverOnAngle : LeverOffAngle;
	ApplyModel();
}

FLinearColor ANLFuseBox::GetFuseColor(int32 Fuse)
{
	static const FLinearColor Colors[NumFuses] =
	{
		FLinearColor(0.85f, 0.08f, 0.06f),  // kırmızı
		FLinearColor(0.06f, 0.28f, 0.95f),  // mavi
		FLinearColor(0.08f, 0.75f, 0.18f),  // yeşil
		FLinearColor(0.95f, 0.78f, 0.05f)   // sarı
	};
	return Colors[FMath::Clamp(Fuse, 0, NumFuses - 1)];
}

FText ANLFuseBox::GetFuseColorName(int32 Fuse)
{
	static const TCHAR* Names[NumFuses] = { TEXT("RED"), TEXT("BLUE"), TEXT("GREEN"), TEXT("YELLOW") };
	return FText::FromString(Names[FMath::Clamp(Fuse, 0, NumFuses - 1)]);
}

FString ANLFuseBox::GetSlotLabel(int32 Slot)
{
	static const TCHAR* Labels[NumFuses] = { TEXT("CAM 1"), TEXT("CAM 2"), TEXT("CAM 3"), TEXT("FLASH") };
	return Labels[FMath::Clamp(Slot, 0, NumFuses - 1)];
}

void ANLFuseBox::BeginPlay()
{
	Super::BeginPlay();
	bOn = bStartOn;

	SlotFuse.SetNum(NumFuses);
	for (int32 i = 0; i < NumFuses; ++i) SlotFuse[i] = i;
	SlotTarget = SlotFuse;

	ApplyModel();
	// Sigortanın renkli bandı (yuva "MI_NL_FuseBand"): BasicShapeMaterial "Color" parametresi
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	FuseMIDs.Reset();
	for (UStaticMeshComponent* Body : FuseBodies)
	{
		UMaterialInstanceDynamic* MID = nullptr;
		if (Body && Base)
		{
			int32 Band = Body->GetMaterialIndex(TEXT("MI_NL_FuseBand"));
			if (Band == INDEX_NONE) Band = 0;
			MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			Body->SetMaterial(Band, MID);
		}
		FuseMIDs.Add(MID);
	}
	LedMIDs.Reset();
	for (UStaticMeshComponent* Led : Leds) LedMIDs.Add(Led ? Led->CreateDynamicMaterialInstance(0) : nullptr);
	NeedleAngles.Init(-58.f, NLFuseLayout::NumGauges);

	ApplyVisuals();
	ApplyFuseVisuals();
	ScheduleNextTrip();
	bWasPowerOk = IsPowerOk();

	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RegisterFuseBox(this);
	}
}

bool ANLFuseBox::AreFusesCorrect() const
{
	for (int32 i = 0; i < SlotFuse.Num(); ++i)
	{
		if (SlotFuse[i] != GetTargetFuse(i)) return false;
	}
	return true;
}

void ANLFuseBox::Interact(ANLPlayerCharacter* Player)
{
	// Sigorta kutusu arayüzünü aç (şalter ve sigortalar orada)
	if (Player) Player->OpenFuseBox(this);
}

FText ANLFuseBox::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return FText::FromString(TEXT("[E] Open fuse box"));
}

void ANLFuseBox::SetOn(bool bNewOn)
{
	if (bOn == bNewOn) return;

	bOn = bNewOn;
	UGameplayStatics::PlaySoundAtLocation(this, bOn ? SwitchOnSound : SwitchOffSound, GetActorLocation());
	ApplyVisuals();
	OnSwitched.Broadcast(bOn);
	RefreshPower();

	// Sigortalar yanlış diziliyken şalter kaldırıldı: kıvılcım, kol geri düşer
	if (bOn && !AreFusesCorrect() && HasActorBegunPlay())
	{
		UE_LOG(LogTemp, Log, TEXT("NLFuseBox: switch raised with wrong wiring -> kicks back"));
		TWeakObjectPtr<ANLFuseBox> WeakThis(this);
		GetWorldTimerManager().SetTimer(KickbackTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			ANLFuseBox* B = WeakThis.Get();
			if (!B || !B->bOn || B->AreFusesCorrect()) return;
			if (UNLPowerSubsystem* P = B->GetWorld()->GetSubsystem<UNLPowerSubsystem>()) P->PlayFarEvent(B->TripSound, B->GetActorLocation());
			B->SetOn(false);
		}), FMath::Max(0.05f, WrongWiringKickback), false);
	}
}

void ANLFuseBox::SwapFuses(int32 SlotA, int32 SlotB)
{
	if (!SlotFuse.IsValidIndex(SlotA) || !SlotFuse.IsValidIndex(SlotB) || SlotA == SlotB) return;
	SlotFuse.Swap(SlotA, SlotB);
	UGameplayStatics::PlaySoundAtLocation(this, FuseMoveSound, GetActorLocation());
	ApplyFuseVisuals();
	RefreshPower();
}

void ANLFuseBox::Sabotage()
{
	SabotageWith(FMath::FRand() < SabotageSwitchOffChance);
}

void ANLFuseBox::SabotageWith(bool bSwitchOff)
{
	// Sigortaları karıştır (doğru dizilime denk gelmesin)
	if (SlotFuse.Num() != NumFuses) return;
	// Kablolamayı değiştir: yeni doğru sıra (monitörde görünür)
	if (bRandomizeWiring && SlotTarget.Num() == NumFuses)
	{
		// Bir öncekiyle aynı sıra çıkmasın
		const TArray<int32> Prev = SlotTarget;
		do
		{
			for (int32 i = NumFuses - 1; i > 0; --i) SlotTarget.Swap(i, FMath::RandRange(0, i));
		} while (SlotTarget == Prev);
	}
	do
	{
		for (int32 i = NumFuses - 1; i > 0; --i)
		{
			SlotFuse.Swap(i, FMath::RandRange(0, i));
		}
	} while (AreFusesCorrect());

	// Şalteri bazen indirir, bazen açık bırakır (açıkken dokunan elektrik çarpar)
	if (bSwitchOff && bOn)
	{
		bOn = false;
		ApplyVisuals();
		OnSwitched.Broadcast(bOn);
	}

	UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
	if (Power) Power->MarkFailureActive();
	UE_LOG(LogTemp, Log, TEXT("NLFuseBox: fuses swapped by saboteur, switch left %s, new wiring [%d %d %d %d] (game time %.0f s)"), bOn ? TEXT("ON") : TEXT("OFF"), SlotTarget[0], SlotTarget[1], SlotTarget[2], SlotTarget[3], GetWorld()->GetTimeSeconds());
	// Sabotaj: kıvılcım + şalter düşmesi kulübe yönünden adanın her yerinden duyulur
	if (Power) Power->PlayFarEvent(TripSound, GetActorLocation());
	if (bSwitchOff && Power) Power->PlayFarEvent(SwitchOffSound, GetActorLocation(), 0.8f);
	ApplyFuseVisuals();
	RefreshPower();
}

void ANLFuseBox::RefreshPower()
{
	if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>())
	{
		Power->RefreshPower();
	}
}

void ANLFuseBox::ApplyVisuals()
{
	// Oyun başında anında yerine koy; sonrasında Tick yumuşakça döndürür
	if (!HasActorBegunPlay())
	{
		LeverAngle = bOn ? LeverOnAngle : LeverOffAngle;
	}
	PlaceLever(LeverAngle);
}

void ANLFuseBox::ApplyFuseVisuals()
{
	for (int32 i = 0; i < FuseMIDs.Num(); ++i)
	{
		if (FuseMIDs[i]) FuseMIDs[i]->SetVectorParameterValue(TEXT("Color"), GetFuseColor(GetFuseInSlot(i)));
	}
}

void ANLFuseBox::PlaceLever(float AngleDeg)
{
	// Kol mafsaldan dışarı çıkar ve yukarı (açık) / aşağı (kapalı) bakar
	const FVector Out = LeverOutward.GetSafeNormal().IsNearlyZero() ? FVector(0.f, 1.f, 0.f) : LeverOutward.GetSafeNormal();
	const float Rad = FMath::DegreesToRadians(AngleDeg);
	const FVector Dir = (Out * FMath::Cos(Rad) + FVector::UpVector * FMath::Sin(Rad)).GetSafeNormal();
	const FQuat Rot = FQuat::FindBetweenNormals(FVector::UpVector, Dir);  // kol mesh'inin uzun ekseni Z
	LeverMesh->SetRelativeLocationAndRotation(LeverPivot + Dir * (LeverLength * 0.5f), Rot);
}

void ANLFuseBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickGauges(DeltaSeconds);

	// Kol hedef konuma döner
	const float TargetAngle = bOn ? LeverOnAngle : LeverOffAngle;
	if (!bLeverDragging && !FMath::IsNearlyEqual(LeverAngle, TargetAngle))
	{
		LeverAngle = FMath::FInterpConstantTo(LeverAngle, TargetAngle, DeltaSeconds, LeverSpeed);
		PlaceLever(LeverAngle);
	}

	// Sigortalar karışıkken diğer arızalar (jeneratör) bekler
	if (!AreFusesCorrect())
	{
		if (UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>()) Power->MarkFailureActive();
	}

	// Elektrik geri geldi (sigortalar doğru + şalter açık): bir sonraki sabotaj sayacı baştan
	const bool bPowerOk = IsPowerOk();
	if (bPowerOk && !bWasPowerOk) ScheduleNextTrip();
	bWasPowerOk = bPowerOk;

	if (!bRandomTrips || !bPowerOk) return;

	// Sadece gün sürerken sayar
	const ANLGameMode* GM = GetWorld()->GetAuthGameMode<ANLGameMode>();
	if (GM && !GM->IsDayActive()) return;

	TripTimer -= DeltaSeconds;
	if (TripTimer <= 0.f)
	{
		// Jeneratör bozuksa / yeni bozulduysa şimdi değil; arıza aralığı dolunca biraz sonra dene
		float Wait = 0.f;
		UNLPowerSubsystem* Power = GetWorld()->GetSubsystem<UNLPowerSubsystem>();
		if (Power && !Power->CanStartFailure(Wait))
		{
			TripTimer = Wait + FMath::FRandRange(5.f, 20.f);
			UE_LOG(LogTemp, Log, TEXT("NLFuseBox: sabotage postponed %.0f s (generator failure active/recent)"), TripTimer);
		}
		else
		{
			// Sabotajcı gelip sigortaları karıştırır; gelemezse hemen karıştır
			TripTimer = 1.0e9f;
			TWeakObjectPtr<ANLFuseBox> WeakThis(this);
			if (!ANLSaboteur::RequestSabotage(GetWorld(), this, [WeakThis]() { if (WeakThis.IsValid()) WeakThis->Sabotage(); }))
			{
				Sabotage();
			}
		}
	}
}

void ANLFuseBox::TickGauges(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bOk = IsPowerOk();
	// İbreler: elektrik varken orta-yukarıda hafifçe titrer, yokken sıfıra düşer. Hangi sigortanın yanlış olduğunu söylemez.
	for (int32 i = 0; i < Needles.Num() && i < NeedleAngles.Num(); ++i)
	{
		const float Jitter = FMath::PerlinNoise1D(Now * 1.3f + i * 7.1f) * 6.f + FMath::PerlinNoise1D(Now * 9.f + i * 3.3f) * 1.5f;
		const float Target = bOk ? (i == 4 ? 28.f : 18.f + i * 4.f) + Jitter : -58.f;
		NeedleAngles[i] = FMath::FInterpTo(NeedleAngles[i], Target, DeltaSeconds, bOk ? 3.f : 6.f);
		if (Needles[i]) Needles[i]->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(NeedleAngles[i])));
	}
	// LED'ler: elektrik varken yeşil yanar (çok hafif dalgalanır), yokken söner
	for (int32 i = 0; i < LedMIDs.Num(); ++i)
	{
		if (!LedMIDs[i]) continue;
		const float K = bOk ? 2.2f * (0.88f + 0.12f * FMath::PerlinNoise1D(Now * 2.f + i * 5.f)) : 0.02f;
		LedMIDs[i]->SetScalarParameterValue(TEXT("Intensity"), K);
	}
}

void ANLFuseBox::ScheduleNextTrip()
{
	TripTimer = FMath::FRandRange(MinTripInterval, FMath::Max(MinTripInterval, MaxTripInterval));
}

// ------------------------------------------------------------------ Kutunun üstünde fareyle etkileşim

static bool RayHitsBox(const FBox& Box, const FVector& Origin, const FVector& Dir)
{
	return FMath::LineBoxIntersection(Box, Origin, Origin + Dir * 1000.f, Dir * 1000.f);
}

int32 ANLFuseBox::RayPickFuse(const FVector& Origin, const FVector& Dir) const
{
	for (int32 i = 0; i < FuseBodies.Num(); ++i)
	{
		if (!FuseBodies[i]) continue;
		// Gövde + başlıklar, biraz pay ile
		FBox B = FuseBodies[i]->Bounds.GetBox();
		B = B.ExpandBy(1.5f);
		if (RayHitsBox(B, Origin, Dir)) return i;
	}
	return INDEX_NONE;
}

bool ANLFuseBox::RayToPanel(const FVector& Origin, const FVector& Dir, FVector& OutLocal) const
{
	// Sigortaların ön yüz düzlemi
	const FTransform& T = BoxMesh->GetComponentTransform();
	const FVector P0 = T.TransformPosition(FVector(0.f, NLFuseLayout::PanelY, 0.f));
	const FVector N = T.TransformVectorNoScale(FVector(0.f, 1.f, 0.f));
	const float Den = FVector::DotProduct(Dir, N);
	if (FMath::IsNearlyZero(Den)) return false;
	const float Dist = FVector::DotProduct(P0 - Origin, N) / Den;
	if (Dist <= 0.f) return false;
	OutLocal = T.InverseTransformPosition(Origin + Dir * Dist);
	return true;
}

int32 ANLFuseBox::RayPickSlot(const FVector& Origin, const FVector& Dir) const
{
	FVector L;
	if (!RayToPanel(Origin, Dir, L)) return INDEX_NONE;
	int32 Best = INDEX_NONE;
	float BestD = 9.f;  // yuva merkezine en fazla bu kadar uzak (cm)
	for (int32 i = 0; i < NumFuses; ++i)
	{
		const FVector S = NLFuseLayout::SlotPos[i];
		// Sigorta boyunca daha toleranslı (dikeyde Z, yatayda X)
		const bool bH = NLFuseLayout::SlotHorizontal[i];
		const float D = FVector2D((L.X - S.X) * (bH ? 0.5f : 1.f), (L.Z - S.Z) * (bH ? 1.f : 0.5f)).Size();
		if (D < BestD) { BestD = D; Best = i; }
	}
	return Best;
}

bool ANLFuseBox::RayHitsLever(const FVector& Origin, const FVector& Dir) const
{
	return LeverMesh && RayHitsBox(LeverMesh->Bounds.GetBox().ExpandBy(5.f), Origin, Dir);
}

void ANLFuseBox::PlaceFuseMeshes(int32 SlotIdx, const FVector& LocalPos, float Scale, bool bHorizontal)
{
	if (!FuseBodies.IsValidIndex(SlotIdx) || !FuseBodies[SlotIdx]) return;
	// Kartuş Z ekseninde; yatay yuvada X eksenine yatırılır
	const FQuat Rot = bHorizontal ? FQuat(FVector::YAxisVector, HALF_PI) : FQuat::Identity;
	FuseBodies[SlotIdx]->SetRelativeLocationAndRotation(LocalPos, Rot);
	FuseBodies[SlotIdx]->SetRelativeScale3D(FVector(Scale));
}

void ANLFuseBox::ResetFuseMeshes()
{
	for (int32 i = 0; i < NumFuses; ++i)
	{
		const bool bHover = i == HoveredSlot && DraggedSlot == INDEX_NONE;
		PlaceFuseMeshes(i, NLFuseLayout::SlotPos[i] + FVector(0.f, bHover ? 1.5f : 0.f, 0.f), bHover ? 1.08f : 1.f, NLFuseLayout::SlotHorizontal[i]);
	}
}

void ANLFuseBox::BeginDrag(int32 SlotIdx)
{
	if (!SlotFuse.IsValidIndex(SlotIdx)) return;
	DraggedSlot = SlotIdx;
	UGameplayStatics::PlaySoundAtLocation(this, FuseMoveSound, GetActorLocation(), 0.6f, 1.3f);
}

void ANLFuseBox::UpdateDrag(const FVector& Origin, const FVector& Dir)
{
	if (DraggedSlot == INDEX_NONE) return;
	FVector L;
	if (!RayToPanel(Origin, Dir, L)) return;
	// Panelin önünde, fareyi takip eder (yuvadan çekilmiş gibi biraz öne çıkık)
	L.Y = NLFuseLayout::PanelY + 7.f;
	L.X = FMath::Clamp(L.X, -28.f, 28.f);
	L.Z = FMath::Clamp(L.Z, -6.f, 74.f);
	PlaceFuseMeshes(DraggedSlot, L, 1.05f, false);
}

void ANLFuseBox::EndDrag()
{
	DraggedSlot = INDEX_NONE;
	ResetFuseMeshes();
}

void ANLFuseBox::SetHoveredSlot(int32 SlotIdx)
{
	if (HoveredSlot == SlotIdx) return;
	HoveredSlot = SlotIdx;
	if (DraggedSlot == INDEX_NONE) ResetFuseMeshes();
}

FVector ANLFuseBox::GetSlotWorldLocation(int32 SlotIdx) const
{
	return BoxMesh->GetComponentTransform().TransformPosition(NLFuseLayout::SlotPos[FMath::Clamp(SlotIdx, 0, NumFuses - 1)]);
}

void ANLFuseBox::SetViewing(bool bViewing)
{
	if (ViewLight) ViewLight->SetIntensity(bViewing ? 900.f : 0.f);
	if (!bViewing) { HoveredSlot = INDEX_NONE; EndDrag(); }
}

void ANLFuseBox::SetLeverDragAngle(float AngleDeg)
{
	bLeverDragging = true;
	const float Lo = FMath::Min(LeverOnAngle, LeverOffAngle);
	const float Hi = FMath::Max(LeverOnAngle, LeverOffAngle);
	LeverAngle = FMath::Clamp(AngleDeg, Lo, Hi);
	PlaceLever(LeverAngle);
}

void ANLFuseBox::EndLeverDrag()
{
	if (!bLeverDragging) return;
	bLeverDragging = false;
	// Yarıyı geçtiyse o tarafa geç; geçmediyse kol eski yerine döner (Tick)
	const float Mid = (LeverOnAngle + LeverOffAngle) * 0.5f;
	const bool bTowardOn = (LeverOnAngle > LeverOffAngle) ? (LeverAngle > Mid) : (LeverAngle < Mid);
	SetOn(bTowardOn);
}

FVector ANLFuseBox::GetLeverWorldLocation() const
{
	return LeverMesh ? LeverMesh->Bounds.Origin : GetActorLocation();
}
