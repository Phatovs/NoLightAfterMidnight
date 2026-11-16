#include "NLLogCabin.h"
#include "NLEnvKit.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"

using namespace NLEnv;

ANLLogCabin::ANLLogCabin()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Static);

	Logs = MakeISM(this, Root, TEXT("Logs"), true);
	Boards = MakeISM(this, Root, TEXT("Boards"), true);
	Shingles = MakeISM(this, Root, TEXT("Shingles"), false);
	Deck = MakeISM(this, Root, TEXT("Deck"), true);
	PaneLit = MakeISM(this, Root, TEXT("PaneLit"), true);
	PaneDark = MakeISM(this, Root, TEXT("PaneDark"), true);
	Lanterns = MakeISM(this, Root, TEXT("Lanterns"), false);
	Pipes = MakeISM(this, Root, TEXT("Pipes"), false);
	Barrels = MakeISM(this, Root, TEXT("Barrels"), true);
	Crates = MakeISM(this, Root, TEXT("Crates"), true);
	NL_KIT_MESH(Logs, "SM_NL_Log");
	NL_KIT_MESH(Boards, "SM_NL_Board");
	NL_KIT_MESH(Shingles, "SM_NL_ShingleRow");
	NL_KIT_MESH(Deck, "SM_NL_DeckPanel");
	NL_KIT_MESH(PaneLit, "SM_NL_Pane");
	NL_KIT_MESH(PaneDark, "SM_NL_Pane");
	NL_KIT_MESH(Lanterns, "SM_NL_Lantern");
	NL_KIT_MESH(Pipes, "SM_NL_Stovepipe");
	NL_KIT_MESH(Barrels, "SM_NL_Barrel");
	NL_KIT_MESH(Crates, "SM_NL_Crate");
	{
		static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Dark(TEXT("/Game/Environment/Materials/MI_NL_WindowDark.MI_NL_WindowDark"));
		if (Dark.Get()) PaneDark->SetMaterial(0, Dark.Get());
	}
	// Küçük parçalar uzakta çizilmez (sis zaten örter)
	for (UInstancedStaticMeshComponent* C : { Lanterns.Get(), Pipes.Get(), Barrels.Get(), Crates.Get() }) C->SetCullDistances(0, 6000);
	Shingles->bCastDynamicShadow = true;

	auto MakeLight = [this](FName Name)
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(Name);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetCastShadows(false);   // performans: kulübe ışıkları gölge atmaz
		L->bUseInverseSquaredFalloff = true;
		return L;
	};
	DoorLight = MakeLight(TEXT("DoorLight"));
	InteriorLight = MakeLight(TEXT("InteriorLight"));

	// Varsayılan: önde kapı + iki yanında pencere, yanda bir pencere
	FNLCabinOpening Door;
	Door.Width = 105.f; Door.Height = 205.f; Door.LanternSide = 1;
	Openings.Add(Door);
	FNLCabinOpening Win;
	Win.bWindow = true; Win.Width = 80.f; Win.Height = 85.f; Win.Sill = 95.f; Win.Offset = -170.f;
	Openings.Add(Win);
	Win.Wall = ENLCabinWall::Right; Win.Offset = 0.f; Win.bLit = false;
	Openings.Add(Win);
}

void ANLLogCabin::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ANLLogCabin::WallFrame(ENLCabinWall W, FVector& Origin, FVector& Dir, FVector& Out, float& HalfLen) const
{
	const float Hw = Width * 0.5f, Hd = Depth * 0.5f;
	switch (W)
	{
	case ENLCabinWall::Front: Origin = FVector(0, -Hd, 0); Dir = FVector(1, 0, 0); Out = FVector(0, -1, 0); HalfLen = Hw; break;
	case ENLCabinWall::Back:  Origin = FVector(0, Hd, 0);  Dir = FVector(1, 0, 0); Out = FVector(0, 1, 0);  HalfLen = Hw; break;
	case ENLCabinWall::Right: Origin = FVector(Hw, 0, 0);  Dir = FVector(0, 1, 0); Out = FVector(1, 0, 0);  HalfLen = Hd; break;
	default:                  Origin = FVector(-Hw, 0, 0); Dir = FVector(0, 1, 0); Out = FVector(-1, 0, 0); HalfLen = Hd; break;
	}
}

float ANLLogCabin::WallLimit(ENLCabinWall W) const
{
	if (!bRuined) return WallHeight;
	const int32 I = static_cast<int32>(W);
	const float H = RuinWallHeights.IsValidIndex(I) ? RuinWallHeights[I] : 0.f;
	return H > 1.f ? FMath::Min(H, WallHeight) : WallHeight;
}

void ANLLogCabin::AddLog(const FVector& Center, const FVector& Dir, float Length, float Diameter)
{
	if (Length < 8.f) return;
	// Kütükler biraz farklı kalınlıkta ve kendi ekseninde dönük (aynı görünmesinler)
	const float D = Diameter * Rng.FRandRange(0.94f, 1.06f);
	const FQuat Spin(FVector::XAxisVector, Rng.FRandRange(0.f, 2.f * PI));
	Add(Logs, Center, Orient(Dir, FVector::UpVector) * Spin, FVector(Length / 100.f, D / 30.f, D / 30.f));
}

void ANLLogCabin::AddBoard(const FVector& Center, const FVector& XDir, const FVector& ZHint, float Length, float WidthY, float Thick)
{
	Add(Boards, Center, Orient(XDir, ZHint), FVector(Length / 100.f, WidthY / 20.f, Thick / 4.f));
}

void ANLLogCabin::Rebuild()
{
	for (UInstancedStaticMeshComponent* C : { Logs.Get(), Boards.Get(), Shingles.Get(), Deck.Get(), PaneLit.Get(), PaneDark.Get(), Lanterns.Get(), Pipes.Get(), Barrels.Get(), Crates.Get() })
	{
		C->ClearInstances();
	}
	DoorLight->SetVisibility(false);
	InteriorLight->SetVisibility(false);
	Rng.Initialize(Seed);

	float WallTop = 0.f;
	BuildWalls(WallTop);
	BuildRoof(WallTop);
	BuildOpenings(WallTop);
	BuildFloorAndProps();
	if (bRuined) BuildRuinDebris();
}

void ANLLogCabin::BuildWalls(float& OutWallTop)
{
	const float D = LogDiameter, R = D * 0.5f, S = D * 0.82f;
	const float Base = R * 0.7f;
	const int32 Rows = FMath::Max(1, FMath::FloorToInt((WallHeight - Base - R) / S) + 1);
	OutWallTop = Base + (Rows - 1) * S + R;
	const float Hd = Depth * 0.5f;
	const float TanP = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(RoofPitch, 10.f, 60.f)));

	for (int32 w = 0; w < 4; ++w)
	{
		const ENLCabinWall W = static_cast<ENLCabinWall>(w);
		const bool bSide = W == ENLCabinWall::Left || W == ENLCabinWall::Right;
		FVector O, Dir, Out;
		float Half;
		WallFrame(W, O, Dir, Out, Half);
		const float Limit = WallLimit(W);
		const bool bFull = Limit >= WallHeight - 1.f;
		// Yan duvarlarda alınlık (üçgen) sadece çatı o tarafı örtüyorsa
		const bool bGable = bSide && bFull && (!bRuined || (W == ENLCabinWall::Left ? RoofCoverage > 0.05f : RoofCoverage > 0.98f));

		const int32 FirstRow = -FMath::CeilToInt(FMath::Max(FoundationDepth, 0.f) / S);
		for (int32 i = FirstRow;; ++i)
		{
			const float Z = Base + i * S - (bSide ? S * 0.5f : 0.f);
			const bool bInWall = Z + R <= FMath::Min(Limit, bSide ? OutWallTop + S * 0.5f : OutWallTop) + 1.f;
			float GableHalf = -1.f;
			if (!bInWall)
			{
				if (!bGable) break;
				// Alınlık: çatının alt yüzeyinin altında kalan kısım
				GableHalf = Hd - (Z + R - OutWallTop) / TanP;
				if (GableHalf < 25.f) break;
			}

			// Bu sıradaki boşluklar (kapı/pencere)
			TArray<FVector2D> Cuts;
			for (const FNLCabinOpening& Op : Openings)
			{
				if (Op.Wall != W) continue;
				if (Z > Op.Sill - R * 0.3f && Z < Op.Sill + Op.Height + R * 0.3f)
				{
					const float Tuck = Op.bFrame ? 4.f : 0.f;
					Cuts.Add(FVector2D(Op.Offset - Op.Width * 0.5f - Tuck, Op.Offset + Op.Width * 0.5f + Tuck));
				}
			}
			Cuts.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });

			const float Ext = GableHalf >= 0.f ? GableHalf : Half + CornerOverhang;
			float A = -Ext;
			TArray<FVector2D> Segs;
			for (const FVector2D& C : Cuts)
			{
				if (C.X > A) Segs.Add(FVector2D(A, C.X));
				A = FMath::Max(A, C.Y);
			}
			Segs.Add(FVector2D(A, Ext));

			const bool bTopRuinRow = bRuined && !bFull && Z + R + S > Limit;
			for (FVector2D Seg : Segs)
			{
				if (Seg.Y - Seg.X < 12.f) continue;
				if (bRuined && Z > WallHeight * 0.4f && Rng.FRand() < MissingLogChance) continue;
				if (bTopRuinRow)
				{
					// Yıkık duvarın en üst kütüğü kırık: bir ucu kısa
					const float Keep = Rng.FRandRange(0.35f, 0.9f);
					if (Rng.FRand() < 0.5f) Seg.Y = Seg.X + (Seg.Y - Seg.X) * Keep; else Seg.X = Seg.Y - (Seg.Y - Seg.X) * Keep;
				}
				const float Mid = (Seg.X + Seg.Y) * 0.5f;
				AddLog(O + Dir * Mid + FVector(0, 0, Z), Dir, Seg.Y - Seg.X, D);
			}
		}
	}
}

void ANLLogCabin::BuildRoof(float WallTop)
{
	const float P = FMath::DegreesToRadians(FMath::Clamp(RoofPitch, 10.f, 60.f));
	const float TanP = FMath::Tan(P), CosP = FMath::Cos(P), SinP = FMath::Sin(P);
	const float Hd = Depth * 0.5f;
	const float LR = Width + 2.f * GableOverhang;
	const float XMin = -LR * 0.5f;
	const float XMax = bRuined ? XMin + LR * FMath::Clamp(RoofCoverage, 0.f, 1.f) : LR * 0.5f;
	if (XMax - XMin < 30.f) return;

	const FVector Ridge(0.f, 0.f, WallTop + Hd * TanP);
	const float Ls = (Hd + EaveOverhang) / CosP;
	const float RowStep = 26.f;

	for (int32 s = -1; s <= 1; s += 2)
	{
		const FVector Dn(0.f, s * CosP, -SinP);
		const FVector Nn(0.f, s * SinP, CosP);
		// Çatı kaplaması (tahta)
		{
			float XE = XMax;
			if (bRuined && XMax < LR * 0.5f) XE -= Rng.FRandRange(0.f, 60.f);
			const FVector C = Ridge + Dn * (Ls * 0.5f) + Nn * 0.f + FVector((XMin + XE) * 0.5f, 0, 0);
			Add(Boards, C, Orient(FVector::XAxisVector, Nn), FVector((XE - XMin) / 100.f, Ls / 20.f, 1.f));
		}
		// Kiremit sıraları (alt sıranın üstüne biner, şaşırtmalı)
		const FVector RowX = s > 0 ? FVector::XAxisVector : -FVector::XAxisVector;
		int32 k = 0;
		for (float d = 0.f; d < Ls - 8.f; d += RowStep, ++k)
		{
			const float Off = (k % 2) * 50.f + Rng.FRandRange(-6.f, 6.f);
			for (float a = XMin - Off; a < XMax; a += 100.f)
			{
				const float a0 = FMath::Max(a, XMin), a1 = FMath::Min(a + 100.f, XMax);
				if (a1 - a0 < 12.f) continue;
				if (bRuined && XMax < LR * 0.5f && a1 > XMax - 140.f && Rng.FRand() < 0.55f) continue;
				const FVector C = Ridge + Dn * d + Nn * (5.5f - 0.3f * FMath::Min(k, 10)) + FVector((a0 + a1) * 0.5f, 0, 0);
				Add(Shingles, C, Orient(RowX, Nn), FVector((a1 - a0) / 100.f, 1.f, 1.f));
			}
		}
		// Aşık kütüğü (alınlıklardan dışarı taşar)
		const float PurlinY = Hd * 0.5f;
		AddLog(FVector((XMin + XMax) * 0.5f + (bRuined ? 0.f : 0.f), s * PurlinY, WallTop + (Hd - PurlinY) * TanP - 13.f), FVector::XAxisVector, XMax - XMin + 20.f, 24.f);
		// Saçak tahtaları (alın kenarı)
		for (float X : { XMin, XMax })
		{
			if (X == XMax && bRuined && XMax < LR * 0.5f) continue;
			AddBoard(Ridge + Dn * (Ls * 0.5f) + Nn * 1.f + FVector(X, 0, 0), Dn, FVector::XAxisVector, Ls + 6.f, 26.f, 4.f);
		}
	}
	// Mahya kütüğü
	AddLog(Ridge + FVector((XMin + XMax) * 0.5f, 0.f, 14.f), FVector::XAxisVector, XMax - XMin + 12.f, 26.f);

	// Soba borusu (arka yamaçtan çıkar)
	if (bChimney)
	{
		const float X = ChimneyPos.X * Width * 0.5f;
		const float Y = FMath::Clamp(ChimneyPos.Y, 0.05f, 0.9f) * Hd;
		if (X > XMin + 20.f && X < XMax - 20.f)
		{
			const float RoofZ = WallTop + (Hd - Y) * TanP + 4.f;
			Add(Pipes, FVector(X, Y, RoofZ - 25.f), FQuat::Identity, FVector::OneVector);
		}
	}
}

void ANLLogCabin::BuildOpenings(float WallTop)
{
	const float D = LogDiameter, R = D * 0.5f;
	bool bDoorLightUsed = false, bAnyLit = false, bPropsDone = false;
	const FVector Up = FVector::UpVector;

	for (const FNLCabinOpening& Op : Openings)
	{
		FVector O, Dir, Out;
		float Half;
		WallFrame(Op.Wall, O, Dir, Out, Half);
		if (bRuined && Op.Sill + Op.Height > WallLimit(Op.Wall)) { /* yıkık duvarda boşluk açık kalır */ }
		const FVector C = O + Dir * Op.Offset;
		const float W = Op.Width, H = Op.Height, Sill = Op.Sill;

		if (Op.bFrame)
		{
			for (int32 e = -1; e <= 1; e += 2)
			{
				AddBoard(C + Dir * (e * (W * 0.5f + 4.f)) + Up * (Sill + H * 0.5f), Up, Dir, H + 8.f, D + 8.f, 8.f);
			}
			AddBoard(C + Up * (Sill + H + 4.f), Dir, Up, W + 16.f, D + 8.f, 8.f);
		}

		if (Op.bWindow)
		{
			if (Op.bFrame) AddBoard(C + Up * (Sill - 3.f) + Out * 5.f, Dir, Up, W + 24.f, D + 16.f, 6.f);
			const bool bLitHere = Op.bLit && !bRuined;
			bAnyLit |= bLitHere;
			Add(bLitHere ? PaneLit : PaneDark, C + Up * (Sill + H * 0.5f), Orient(Dir, Up), FVector(W / 100.f, 1.f, H / 100.f));
			// Kayıtlar (artı şeklinde)
			AddBoard(C + Out * 3.5f + Up * (Sill + H * 0.5f), Up, Out, H, 4.f, 4.f);
			AddBoard(C + Out * 3.5f + Up * (Sill + H * 0.55f), Dir, Out, W - 1.f, 4.f, 3.f);
			if (Op.bShutters)
			{
				const float A = FMath::DegreesToRadians(Rng.FRandRange(8.f, 22.f));
				for (int32 e = -1; e <= 1; e += 2)
				{
					const FVector Hinge = C + Dir * (e * (W * 0.5f + 8.f)) + Out * (R + 3.f) + Up * (Sill + H * 0.5f);
					const FVector PDir = Dir * (e * FMath::Cos(A)) + Out * FMath::Sin(A);
					const FVector PN = Out * FMath::Cos(A) - Dir * (e * FMath::Sin(A));
					const float Bw = W * 0.5f / 3.f;
					for (int32 b = 0; b < 3; ++b)
					{
						AddBoard(Hinge + PDir * (Bw * (b + 0.5f)), Up, PN, H + 6.f, Bw * 0.94f, 3.f);
					}
					for (float Hz : { 0.22f, 0.78f })
					{
						AddBoard(Hinge + PDir * (W * 0.25f) + PN * 3.f + Up * ((Hz - 0.5f) * H), PDir, PN, W * 0.5f, 9.f, 3.f);
					}
				}
			}
			continue;
		}

		// Kapı
		if (Op.bDoorPanel)
		{
			const int32 Nb = FMath::Max(3, FMath::RoundToInt(W / 20.f));
			const float Bw = W / Nb;
			for (int32 b = 0; b < Nb; ++b)
			{
				AddBoard(C + Dir * (-W * 0.5f + Bw * (b + 0.5f)) - Out * (R * 0.3f) + Up * (Sill + H * 0.5f), Up, Out, H - 2.f, Bw * 0.96f, 4.f);
			}
			for (float Hz : { 0.18f, 0.82f })
			{
				AddBoard(C - Out * (R * 0.3f + 4.f) + Up * (Sill + H * Hz), Dir, Out, W - 10.f, 14.f, 3.f);
			}
		}
		if (Op.LanternSide != 0)
		{
			const float E = Op.LanternSide > 0 ? 1.f : -1.f;
			const FVector Base = C + Dir * (E * (W * 0.5f + 42.f)) + Up * (Sill + H + 6.f) + Out * R;
			AddBoard(Base + Out * 22.f, Out, Up, 46.f, 6.f, 6.f);
			const FVector LanternAt = Base + Out * 40.f - Up * 3.f;
			Add(Lanterns, LanternAt, Orient(Dir, Up), FVector::OneVector);
			if (!bDoorLightUsed && !bRuined)
			{
				bDoorLightUsed = true;
				DoorLight->SetRelativeLocation(LanternAt - Up * 24.f);
				DoorLight->SetIntensity(LanternIntensity);
				DoorLight->SetAttenuationRadius(LanternRadius);
				DoorLight->SetLightColor(LampColor);
				DoorLight->SetVisibility(true);
			}
		}
		if (bPorchProps && !bPropsDone)
		{
			bPropsDone = true;
			const float E = Op.LanternSide > 0 ? -1.f : 1.f;
			const FVector P0 = C + Dir * (E * (W * 0.5f + 85.f)) + Out * (R + 45.f);
			Add(Barrels, P0, FQuat(Up, Rng.FRandRange(0.f, 6.28f)), FVector::OneVector);
			Add(Barrels, P0 + Dir * (E * 62.f) + Out * 8.f, FQuat(Up, Rng.FRandRange(0.f, 6.28f)), FVector::OneVector);
			Add(Crates, P0 + Out * 75.f + Dir * (E * 20.f), FQuat(Up, Rng.FRandRange(-0.4f, 0.4f)), FVector::OneVector);
		}
	}

	if (bAnyLit && bInteriorLamp)
	{
		InteriorLight->SetRelativeLocation(FVector(0, 0, WallTop * 0.72f));
		InteriorLight->SetIntensity(InteriorIntensity);
		InteriorLight->SetAttenuationRadius(FMath::Max(Width, Depth) * 0.9f);
		InteriorLight->SetLightColor(LampColor);
		InteriorLight->SetVisibility(true);
	}
}

void ANLLogCabin::BuildFloorAndProps()
{
	if (!bFloor) return;
	const float Hw = Width * 0.5f, Hd = Depth * 0.5f;
	for (float X = -Hw; X < Hw - 1.f; X += 200.f)
	{
		const float X1 = FMath::Min(X + 200.f, Hw);
		for (float Y = -Hd; Y < Hd - 1.f; Y += 100.f)
		{
			const float Y1 = FMath::Min(Y + 100.f, Hd);
			Add(Deck, FVector((X + X1) * 0.5f, (Y + Y1) * 0.5f, 1.f), FQuat::Identity, FVector((X1 - X) / 200.f, (Y1 - Y) / 100.f, 1.f));
		}
	}
}

void ANLLogCabin::BuildRuinDebris()
{
	const float Hw = Width * 0.5f, Hd = Depth * 0.5f;
	for (int32 i = 0; i < FallenLogs; ++i)
	{
		const float Yaw = Rng.FRandRange(0.f, 2.f * PI);
		const float Pitch = FMath::DegreesToRadians(Rng.FRandRange(0.f, 22.f));
		const FVector Dir(FMath::Cos(Yaw) * FMath::Cos(Pitch), FMath::Sin(Yaw) * FMath::Cos(Pitch), FMath::Sin(Pitch));
		const float L = Rng.FRandRange(140.f, 340.f);
		const FVector C(Rng.FRandRange(-Hw, Hw) * 0.7f, Rng.FRandRange(-Hd, Hd) * 0.7f, LogDiameter * 0.5f + Dir.Z * L * 0.5f);
		AddLog(C, Dir, L, LogDiameter);
	}
	// Çatıdan düşmüş tahtalar
	for (int32 i = 0; i < 6; ++i)
	{
		const float Yaw = Rng.FRandRange(0.f, 2.f * PI);
		const FVector Dir(FMath::Cos(Yaw), FMath::Sin(Yaw), Rng.FRandRange(-0.05f, 0.25f));
		AddBoard(FVector(Rng.FRandRange(-Hw, Hw) * 0.8f, Rng.FRandRange(-Hd, Hd) * 0.8f, 6.f), Dir, FVector::UpVector, Rng.FRandRange(80.f, 200.f), 20.f, 3.f);
	}
}
