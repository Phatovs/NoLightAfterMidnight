#include "NLSecurityDesk.h"
#include "NLDefaults.h"
#include "NLSecurityCamera.h"
#include "NLPowerConsumerComponent.h"
#include "NLPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"

ANLSecurityDesk::ANLSecurityDesk()
{
	// Varsayılan sesler (Content/Audio/NL)
	NL_DEFAULT_SOUND(CameraSwitchSound, "camera_switch");
	NL_DEFAULT_SOUND(ComputerOnSound, "computer_on");
	NL_DEFAULT_SOUND(ComputerOffSound, "computer_off");
	NL_DEFAULT_SOUND(MonitorHumLoop, "monitor_hum_loop");
	NL_DEFAULT_SOUND(SitDownSound, "chair_sit");

	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	DeskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeskMesh"));
	DeskMesh->SetupAttachment(Root);

	MonitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonitorMesh"));
	MonitorMesh->SetupAttachment(Root);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(Root);
	ViewCamera->SetRelativeLocation(FVector(-80.f, 0.f, 120.f));

	SeatPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(Root);
	SeatPoint->SetRelativeLocation(FVector(-90.f, 0.f, 90.f));

	// Basit sandalye (oyuncu içinden geçebilir; oturunca kalça oturağa iner)
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ChairSeat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairSeat"));
	ChairSeat->SetupAttachment(Root);
	ChairSeat->SetStaticMesh(CubeFinder.Get());
	ChairSeat->SetRelativeLocation(FVector(-76.f, 0.f, SeatHeight - 4.f));
	ChairSeat->SetRelativeScale3D(FVector(0.46f, 0.46f, 0.08f));
	ChairSeat->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// Ahşap sandalye modeli varsa (Scripts/nl_env_kit.py): oturak bileşeni bütün sandalye olur, ayrı sırt gizlenir
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> ChairFinder(TEXT("/Game/Environment/Kit/SM_NL_Chair.SM_NL_Chair"));
	const bool bChairModel = ChairFinder.Get() != nullptr;
	if (bChairModel)
	{
		ChairSeat->SetStaticMesh(ChairFinder.Get());
		ChairSeat->SetRelativeScale3D(FVector::OneVector);
	}

	ChairBack = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairBack"));
	ChairBack->SetupAttachment(Root);
	ChairBack->SetStaticMesh(CubeFinder.Get());
	ChairBack->SetRelativeLocation(FVector(-101.f, 0.f, SeatHeight + 28.f));
	ChairBack->SetRelativeScale3D(FVector(0.06f, 0.46f, 0.5f));
	ChairBack->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (bChairModel) ChairBack->SetStaticMesh(nullptr);

	MonitorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MonitorAudio"));
	MonitorAudio->SetupAttachment(MonitorMesh);
	MonitorAudio->bAutoActivate = false;

	PowerConsumer = CreateDefaultSubobject<UNLPowerConsumerComponent>(TEXT("PowerConsumer"));
}

void ANLSecurityDesk::BeginPlay()
{
	Super::BeginPlay();
	PowerConsumer->OnPowerStateChanged.AddDynamic(this, &ANLSecurityDesk::HandlePowerChanged);

	if (!FeedRenderTarget)
	{
		FeedRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_CameraFeed_Auto"));
		FeedRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		FeedRenderTarget->ClearColor = FLinearColor::Black;
		FeedRenderTarget->InitAutoFormat(AutoFeedResolution.X, AutoFeedResolution.Y);
		FeedRenderTarget->UpdateResourceImmediate(true);
	}

	MonitorMID = MonitorMesh->GetStaticMesh() ? MonitorMesh->CreateDynamicMaterialInstance(MonitorMaterialIndex) : nullptr;
	if (MonitorMID && FeedRenderTarget)
	{
		MonitorMID->SetTextureParameterValue(FeedTextureParam, FeedRenderTarget);
	}
	if (MonitorHumLoop)
	{
		MonitorAudio->SetSound(MonitorHumLoop);
	}

	Cameras.RemoveAll([](const TObjectPtr<ANLSecurityCamera>& Cam) { return Cam == nullptr; });
	ActiveIndex = 0;

	// Başlangıç durumunu sessizce uygula
	if (MonitorMID) MonitorMID->SetScalarParameterValue(PowerParam, IsPowered() ? 1.f : 0.f);
	if (IsPowered()) MonitorAudio->Play();
	RefreshFeed();
}

void ANLSecurityDesk::Interact(ANLPlayerCharacter* Player)
{
	if (Player && !bPlayerSeated)
	{
		Player->SitAtDesk(this);
	}
}

FText ANLSecurityDesk::GetInteractPrompt(const ANLPlayerCharacter* Player) const
{
	return bPlayerSeated ? FText::GetEmpty() : FText::FromString(TEXT("[E] Sit at desk"));
}

void ANLSecurityDesk::SwitchToCamera(int32 Index)
{
	if (!IsPowered() || !Cameras.IsValidIndex(Index) || Index == ActiveIndex) return;

	ActiveIndex = Index;
	UGameplayStatics::PlaySound2D(this, CameraSwitchSound);
	RefreshFeed();
	OnActiveCameraChanged.Broadcast(ActiveIndex);
}

void ANLSecurityDesk::NextCamera()
{
	if (Cameras.Num() > 0) SwitchToCamera((ActiveIndex + 1) % Cameras.Num());
}

void ANLSecurityDesk::PreviousCamera()
{
	if (Cameras.Num() > 0) SwitchToCamera((ActiveIndex - 1 + Cameras.Num()) % Cameras.Num());
}

bool ANLSecurityDesk::FlashActiveCamera()
{
	ANLSecurityCamera* Cam = GetActiveCamera();
	return Cam && Cam->Flash();
}

ANLSecurityCamera* ANLSecurityDesk::GetActiveCamera() const
{
	return Cameras.IsValidIndex(ActiveIndex) ? Cameras[ActiveIndex].Get() : nullptr;
}

bool ANLSecurityDesk::IsPowered() const
{
	return PowerConsumer && PowerConsumer->IsPowered();
}

FTransform ANLSecurityDesk::GetSeatTransform() const
{
	return SeatPoint->GetComponentTransform();
}

void ANLSecurityDesk::OnPlayerSeated()
{
	bPlayerSeated = true;
	UGameplayStatics::PlaySoundAtLocation(this, SitDownSound, SeatPoint->GetComponentLocation());
}

void ANLSecurityDesk::OnPlayerLeft()
{
	bPlayerSeated = false;
}

void ANLSecurityDesk::HandlePowerChanged(bool bHasPower)
{
	if (MonitorMID)
	{
		MonitorMID->SetScalarParameterValue(PowerParam, bHasPower ? 1.f : 0.f);
	}

	// Oyunun ilk anlarında (sistemler kayıt olurken) açılış/kapanış sesi çalma
	const bool bPlaySounds = GetWorld()->GetTimeSeconds() > 1.f;

	if (bHasPower)
	{
		if (bPlaySounds) UGameplayStatics::PlaySoundAtLocation(this, ComputerOnSound, MonitorMesh->GetComponentLocation());
		MonitorAudio->Play();
	}
	else
	{
		if (bPlaySounds) UGameplayStatics::PlaySoundAtLocation(this, ComputerOffSound, MonitorMesh->GetComponentLocation());
		MonitorAudio->Stop();
	}

	RefreshFeed();
}

void ANLSecurityDesk::RefreshFeed()
{
	for (int32 i = 0; i < Cameras.Num(); ++i)
	{
		if (Cameras[i])
		{
			Cameras[i]->SetFeedActive(i == ActiveIndex && IsPowered(), FeedRenderTarget);
		}
	}
}

FVector ANLSecurityDesk::GetSitPelvisLocation() const
{
	FVector P = ChairSeat->GetComponentLocation();
	P.Z = GetFloorZ() + SeatHeight + 10.f;
	return P;
}

FVector ANLSecurityDesk::GetMonitorLocation() const
{
	return MonitorMesh->Bounds.Origin;
}
