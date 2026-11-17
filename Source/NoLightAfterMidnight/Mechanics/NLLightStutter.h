// NoLightAfterMidnight - Eski lambaların ara ara "teklemesi" (göz kırpması)
// Rastgele aralıklarla 1-3 hızlı kırpma, bazen yarım saniyeye yakın kararma. Update() ışık çarpanı döner (1 = normal).
#pragma once

#include "CoreMinimal.h"

struct FNLLightStutter
{
	// İki tekleme arası (saniye)
	FVector2D Interval = FVector2D(12.f, 30.f);

	float Update(float Now)
	{
		if (NextAt < 0.f) NextAt = Now + FMath::FRandRange(Interval.X, Interval.Y) * FMath::FRandRange(0.3f, 1.f);
		if (Keys.Num() == 0)
		{
			if (Now < NextAt) return 1.f;
			Build(Now);
		}
		const float T = Now - Start;
		for (const FVector2D& K : Keys)
		{
			if (T < K.X) return K.Y;
		}
		Keys.Reset();
		NextAt = Now + FMath::FRandRange(Interval.X, Interval.Y);
		return 1.f;
	}

	bool IsActive() const { return Keys.Num() > 0; }

private:
	float NextAt = -1.f;
	float Start = 0.f;
	TArray<FVector2D> Keys; // (bitiş zamanı, ışık seviyesi)

	void Build(float Now)
	{
		Start = Now;
		float T = 0.f;
		if (FMath::FRand() < 0.2f)
		{
			// Uzun kırpma: cızırtı, yarım saniyeye yakın karanlık, geri gelirken bir cızırtı daha
			T += FMath::FRandRange(0.04f, 0.08f); Keys.Add(FVector2D(T, 0.35f));
			T += FMath::FRandRange(0.05f, 0.09f); Keys.Add(FVector2D(T, 0.9f));
			T += FMath::FRandRange(0.3f, 0.6f);   Keys.Add(FVector2D(T, 0.03f));
			T += FMath::FRandRange(0.05f, 0.1f);  Keys.Add(FVector2D(T, 0.6f));
			T += FMath::FRandRange(0.04f, 0.08f); Keys.Add(FVector2D(T, 0.15f));
			return;
		}
		const int32 Blinks = FMath::RandRange(1, 3);
		for (int32 i = 0; i < Blinks; ++i)
		{
			T += FMath::FRandRange(0.04f, 0.12f);
			Keys.Add(FVector2D(T, FMath::FRandRange(0.f, 0.25f)));
			if (i + 1 < Blinks)
			{
				T += FMath::FRandRange(0.05f, 0.15f);
				Keys.Add(FVector2D(T, FMath::FRandRange(0.7f, 1.f)));
			}
		}
	}
};
