// NoLightAfterMidnight - HUD: ilk gecenin hedefi
// Oyuncu rıhtıma indikten sonra ("DAY 1" yazısı kaybolunca) sol üstte küçük, soluk bir hedef yazısı belirir:
// "Get to the lighthouse." Deniz fenerindeki güvenlik masasına yaklaşınca ya da masaya oturunca söner, bir daha çıkmaz.
// Asıl yön gösterenler oyun dünyasında: dönen fener ışığı, patika boyunca yanan fenerli direkler, rıhtım çıkışındaki tabela.
#include "NLHUDWidget.h"
#include "NLPlayerCharacter.h"
#include "NLGameMode.h"
#include "NLSecurityDesk.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"

void UNLHUDWidget::BuildObjective(UCanvasPanel* Root)
{
	ObjectiveText = MakeText(Root, TEXT("ObjectiveText"), TEXT("Get to the lighthouse."), 18, FVector2D(0, 0), FVector2D(0, 0), FVector2D(40, 36), FLinearColor(0.85f, 0.82f, 0.74f, 1.f));
	ObjectiveText->SetShadowOffset(FVector2D(1.f, 1.f));
	ObjectiveText->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
	ObjectiveText->SetRenderOpacity(0.f);
	SetShown(ObjectiveText, false);
}

void UNLHUDWidget::TickObjective(float InDeltaTime, const ANLPlayerCharacter* Player, const ANLGameMode* GM)
{
	if (!ObjectiveText) return;
	bool bWant = false;
	if (!bObjectiveDone && Player && GM && GM->GetCurrentDay() == 1 && GM->IsDayActive())
	{
		// "DAY 1" yazısı bitsin
		const float SinceStart = GM->GetDayProgress() * 600.f;
		const bool bAfterBanner = SinceStart > 0.f && CenterMessageTimer <= 0.f;
		const ANLSecurityDesk* Desk = nullptr;
		for (TActorIterator<ANLSecurityDesk> It(GetWorld()); It; ++It) { Desk = *It; break; }
		const bool bArrived = Player->GetMode() == ENLPlayerMode::Seated
			|| (Desk && FVector::Dist2D(Player->GetActorLocation(), Desk->GetActorLocation()) < 900.f);
		if (bArrived)
		{
			bObjectiveDone = true;
			UE_LOG(LogTemp, Log, TEXT("NLObjective: reached the lighthouse"));
		}
		bWant = bAfterBanner && !bObjectiveDone && Player->GetMode() != ENLPlayerMode::Dead;
	}
	ObjectiveAlpha = FMath::FInterpConstantTo(ObjectiveAlpha, bWant ? 1.f : 0.f, InDeltaTime, bWant ? 0.7f : 1.2f);
	ObjectiveText->SetRenderOpacity(ObjectiveAlpha * 0.85f);
	SetShown(ObjectiveText, ObjectiveAlpha > 0.001f);
}
