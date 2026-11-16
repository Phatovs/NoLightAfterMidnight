// NoLightAfterMidnight - Sabotajcı: şalteri indiren / jeneratörü bozan görünür karakter
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NLSaboteur.generated.h"

class UNLBodyRigComponent;

/**
 * Şalter atma / jeneratör arıza zamanı gelince bu karakter görünür:
 *  1) oyuncunun görmediği bir yerde belirir, çömelerek sinsice hedefe yürür
 *  2) hedefi elleriyle kurcalar (alçak hedefte eğilir); arıza bu sırada olur
 *  3) doğrulup koşarak görüş dışına kaçar ve kaybolur
 * Sahneye konması gerekmez: ilk ihtiyaçta kendiliğinden oluşturulur.
 * Pozlar NLBodyRigComponent ile kodla verilir; GetMesh() görünmez sürücüdür (yürüme/koşu animasyonu).
 */
UCLASS()
class NOLIGHTAFTERMIDNIGHT_API ANLSaboteur : public ACharacter
{
	GENERATED_BODY()

public:
	ANLSaboteur();
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Boştaki bir sabotajcıyı hedefe gönderir (yoksa oluşturur). Apply, kurcalama sırasında çağrılır.
	 * Sabotajcı gönderilemezse false döner; çağıran arızayı hemen uygulamalıdır.
	 */
	static bool RequestSabotage(UWorld* World, AActor* Target, TFunction<void()> Apply);

	bool IsBusy() const { return State != EState::Idle; }
	// Test için durum: 0 boşta, 1 sinsice yürüyor, 2 kurcalıyor, 3 kaçıyor
	int32 GetDebugState() const { return int32(State); }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "NL|Components") TObjectPtr<UNLBodyRigComponent> BodyRig;

	// Sinsice yürüme ve kaçış hızları (oyuncu 280)
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float SneakSpeed = 150.f;
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float FleeSpeed = 520.f;
	// Hedefi kurcalama süresi; arıza bu sürenin sonuna doğru olur
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float TamperDuration = 2.6f;
	// Kurcalarken hedefe ne kadar yakın durur (gövde merkezinden elin değdiği noktaya, cm)
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float TamperReach = 42.f;
	// Hedeften bu aralıkta (cm), oyuncunun görmediği bir yerde belirir
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float SpawnMinDistance = 900.f;
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float SpawnMaxDistance = 2200.f;
	// Sinsice yürürken ne kadar çömelir (cm)
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float SneakCrouchDepth = 34.f;
	// Hedefe bu sürede ulaşamazsa arızayı yine de uygular ve kaybolur (oyun akışı bozulmasın)
	UPROPERTY(EditAnywhere, Category = "NL|Saboteur") float GiveUpTime = 30.f;

private:
	enum class EState : uint8 { Idle, Sneaking, Tampering, Fleeing };
	EState State = EState::Idle;
	float StateTime = 0.f;
	TWeakObjectPtr<AActor> Target;
	TFunction<void()> ApplyFn;
	bool bApplied = false;
	FVector ApproachPoint = FVector::ZeroVector;
	FVector HandPoint = FVector::ZeroVector;
	FVector FleePoint = FVector::ZeroVector;
	float Crouch = 0.f;
	float Hands = 0.f;

	void Begin(AActor* InTarget, TFunction<void()> Apply);
	void EnterState(EState NewState);
	void Apply();
	void Vanish();
	void MoveTo(const FVector& Point, float Speed);
	bool FindHiddenNavPoint(const FVector& Around, float MinDist, float MaxDist, FVector& Out) const;
	bool FindApproachPoint(const AActor* InTarget, FVector& Out) const;
};
