#include "Game/CLGreyboxLayouts.h"
#include "Game/CLGreyboxFloors.h"
#include "AI/CLTaskMarker.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Math/RotationMatrix.h"
#include "Engine/World.h"

namespace
{
	struct FRaidRingSpec
	{
		FName Id;
		float DiameterM = 5.f;
		float ElevateCm = 0.f;
	};

	struct FRaidChamberSpec
	{
		FName Id;
		FString Pattern;
		TArray<FRaidRingSpec> Rings;
		FRaidRingSpec Main;
		FName Portal;
		FName Door;
	};

	struct FRaidObeliskRecipe
	{
		float ChamberM = 40.f;
		float GapM = 12.f;
		float WallH = 8.f;
		float WallSlopeDeg = 5.f;
		float ObeliskHeightM = 90.f;
		float ObeliskInsetM = 5.f;
		int32 OrbitCount = 8;
		TArray<FRaidChamberSpec> Chambers;

		bool Load()
		{
			const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Greybox/RaidObelisk.json"));
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
					ECLErrorKind::NonDeterministic, TEXT("raid_recipe_missing"), Path));
				return false;
			}
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
					ECLErrorKind::NonDeterministic, TEXT("raid_recipe_parse"), Path));
				return false;
			}
			if (Root->HasField(TEXT("chamberM"))) ChamberM = static_cast<float>(Root->GetNumberField(TEXT("chamberM")));
			if (Root->HasField(TEXT("gapM"))) GapM = static_cast<float>(Root->GetNumberField(TEXT("gapM")));
			if (Root->HasField(TEXT("wallH"))) WallH = static_cast<float>(Root->GetNumberField(TEXT("wallH")));
			if (Root->HasField(TEXT("wallSlopeDeg"))) WallSlopeDeg = static_cast<float>(Root->GetNumberField(TEXT("wallSlopeDeg")));
			if (Root->HasField(TEXT("obeliskHeightM"))) ObeliskHeightM = static_cast<float>(Root->GetNumberField(TEXT("obeliskHeightM")));
			if (Root->HasField(TEXT("obeliskInsetM"))) ObeliskInsetM = static_cast<float>(Root->GetNumberField(TEXT("obeliskInsetM")));
			if (Root->HasField(TEXT("orbitCount"))) OrbitCount = static_cast<int32>(Root->GetNumberField(TEXT("orbitCount")));
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (!Root->TryGetArrayField(TEXT("chambers"), Arr) || !Arr)
			{
				return true;
			}
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject> Ch = V.IsValid() ? V->AsObject() : nullptr;
				if (!Ch.IsValid()) continue;
				FRaidChamberSpec Spec;
				FString IdS;
				Ch->TryGetStringField(TEXT("id"), IdS);
				Spec.Id = FName(*IdS);
				Ch->TryGetStringField(TEXT("pattern"), Spec.Pattern);
				FString PortalS, DoorS;
				if (Ch->TryGetStringField(TEXT("portal"), PortalS)) Spec.Portal = FName(*PortalS);
				if (Ch->TryGetStringField(TEXT("door"), DoorS)) Spec.Door = FName(*DoorS);
				auto ReadRing = [](const TSharedPtr<FJsonObject>& Obj, FRaidRingSpec& Out)
				{
					FString Rid;
					Obj->TryGetStringField(TEXT("id"), Rid);
					Out.Id = FName(*Rid);
					if (Obj->HasField(TEXT("diameterM"))) Out.DiameterM = static_cast<float>(Obj->GetNumberField(TEXT("diameterM")));
					if (Obj->HasField(TEXT("elevateCm"))) Out.ElevateCm = static_cast<float>(Obj->GetNumberField(TEXT("elevateCm")));
				};
				const TArray<TSharedPtr<FJsonValue>>* Rings = nullptr;
				if (Ch->TryGetArrayField(TEXT("rings"), Rings) && Rings)
				{
					for (const TSharedPtr<FJsonValue>& RV : *Rings)
					{
						const TSharedPtr<FJsonObject> RObj = RV.IsValid() ? RV->AsObject() : nullptr;
						if (!RObj.IsValid()) continue;
						FRaidRingSpec Ring;
						ReadRing(RObj, Ring);
						Spec.Rings.Add(Ring);
					}
				}
				const TSharedPtr<FJsonObject>* Main = nullptr;
				if (Ch->TryGetObjectField(TEXT("main"), Main) && Main && Main->IsValid())
				{
					ReadRing(*Main, Spec.Main);
				}
				Chambers.Add(MoveTemp(Spec));
			}
			return true;
		}
	};

	FVector ChamberOrigin(const FRaidObeliskRecipe& R, int32 Index)
	{
		return FVector(Index * (R.ChamberM + R.GapM) * 100.f, 0.f, 0.f);
	}

	void PlaceRingLocals(const FRaidChamberSpec& Spec, TArray<FVector>& OutRings, FVector& OutMain)
	{
		OutRings.Reset();
		OutMain = FVector::ZeroVector;
		const float Arm = 1200.f;
		if (Spec.Pattern.Equals(TEXT("square"), ESearchCase::IgnoreCase))
		{
			OutRings.Add(FVector(Arm, Arm, 0.f));
			OutRings.Add(FVector(Arm, -Arm, 0.f));
			OutRings.Add(FVector(-Arm, -Arm, 0.f));
			OutMain = FVector::ZeroVector;
			return;
		}
		if (Spec.Pattern.Equals(TEXT("line"), ESearchCase::IgnoreCase))
		{
			OutRings.Add(FVector(-1200.f, 0.f, 0.f));
			OutRings.Add(FVector(-200.f, 0.f, 0.f));
			OutRings.Add(FVector(800.f, 0.f, 0.f));
			OutMain = FVector(1600.f, 0.f, 0.f);
			return;
		}
		for (int32 i = 0; i < 3; ++i)
		{
			const float Deg = 90.f + 120.f * static_cast<float>(i);
			const float Rad = FMath::DegreesToRadians(Deg);
			OutRings.Add(FVector(FMath::Cos(Rad) * Arm, FMath::Sin(Rad) * Arm, 0.f));
		}
		OutMain = FVector::ZeroVector;
	}

	void StampOccupyPad(ACLGreyboxFloors& Floors, const FVector& Center, const FRaidRingSpec& Ring, bool bCover)
	{
		const float Diam = FMath::Max(2.f, Ring.DiameterM);
		if (Ring.ElevateCm > 50.f)
		{
			Floors.AddPlatform(Center, Diam, Diam, 40.f);
		}
		else
		{
			Floors.AddBox(
				FVector(Center.X, Center.Y, Center.Z - 20.f),
				FVector(Diam * 100.f, Diam * 100.f, 40.f),
				FRotator::ZeroRotator,
				true,
				false);
		}
		if (bCover)
		{
			Floors.StampModule(FName(TEXT("cover_half")), Center + FVector(180.f, 0.f, 42.f));
			Floors.StampModule(FName(TEXT("cover_half")), Center + FVector(-180.f, 160.f, 42.f), FRotator(0.f, 90.f, 0.f));
		}
	}

	void StampOccupyMarkers(UWorld* World, const FVector& Center, const FRaidRingSpec& Ring, int32 OrbitCount)
	{
		if (!World) return;
		const float Diam = FMath::Max(2.f, Ring.DiameterM);
		const float Zone = Diam * 50.f + 40.f;
		ACLTaskMarker::SpawnAt(World, Ring.Id, Center, Zone);
		const float OrbitR = Diam * 50.f * 0.82f;
		for (int32 i = 0; i < OrbitCount; ++i)
		{
			const float Deg = 360.f * static_cast<float>(i) / static_cast<float>(FMath::Max(1, OrbitCount));
			const float Rad = FMath::DegreesToRadians(Deg);
			const FVector Crumb = Center + FVector(FMath::Cos(Rad) * OrbitR, FMath::Sin(Rad) * OrbitR, 0.f);
			ACLTaskMarker::SpawnAt(World, FName(*FString::Printf(TEXT("%s_orbit_%d"), *Ring.Id.ToString(), i)), Crumb, 90.f);
		}
	}

	void AddSlopedWallPiece(ACLGreyboxFloors& Floors, const FVector& Hinge, const FVector2D& OutXY,
		float Along, float Thick, float Height, float TangentShift, float HeightFromFloor, float SlopeDeg)
	{
		const float Rad = FMath::DegreesToRadians(SlopeDeg);
		const float S = FMath::Sin(Rad);
		const float C = FMath::Cos(Rad);
		const FVector WallUp(OutXY.X * S, OutXY.Y * S, C);
		const FVector WallOut(OutXY.X * C, OutXY.Y * C, -S);
		const FVector Tangent = FVector::CrossProduct(WallOut, WallUp);
		const FRotator Rot = FRotationMatrix::MakeFromZX(WallUp, Tangent).Rotator();
		const float U = Thick * 0.5f;
		const float V = HeightFromFloor + Height * 0.5f;
		const float U2 = U * C + V * S;
		const float V2 = -U * S + V * C;
		const FVector Center = Hinge
			+ Tangent * TangentShift
			+ FVector(OutXY.X, OutXY.Y, 0.f) * U2
			+ FVector(0.f, 0.f, V2);
		Floors.AddBox(Center, FVector(Along, Thick, Height), Rot);
	}

	void StampChamberShell(ACLGreyboxFloors& Floors, const FRaidObeliskRecipe& R, const FVector& Origin, bool bWestArch, bool bEastArch, FName DoorId)
	{
		const float M = 100.f;
		const float Half = R.ChamberM * 0.5f * M;
		Floors.StampFillFloor(Origin, R.ChamberM, R.ChamberM, 40.f);
		const float WallH = R.WallH * M;
		const float Thick = 80.f;
		const float SlopeDeg = R.WallSlopeDeg;
		const float Extra = WallH * FMath::Tan(FMath::DegreesToRadians(SlopeDeg));
		const float Along = R.ChamberM * M + Thick * 2.f + Extra * 2.f;
		const float Inset = R.ObeliskInsetM * M;
		const float ObeliskH = R.ObeliskHeightM * M;
		const FVector Corners[4] = {
			Origin + FVector(Half - Inset, Half - Inset, ObeliskH * 0.5f),
			Origin + FVector(-(Half - Inset), Half - Inset, ObeliskH * 0.5f),
			Origin + FVector(Half - Inset, -(Half - Inset), ObeliskH * 0.5f),
			Origin + FVector(-(Half - Inset), -(Half - Inset), ObeliskH * 0.5f)
		};
		for (const FVector& C : Corners)
		{
			Floors.StampModule(FName(TEXT("obelisk")), C);
		}
		const FVector HingeN = Origin + FVector(0.f, Half, 0.f);
		const FVector HingeS = Origin + FVector(0.f, -Half, 0.f);
		AddSlopedWallPiece(Floors, HingeN, FVector2D(0.f, 1.f), Along, Thick, WallH, 0.f, 0.f, SlopeDeg);
		AddSlopedWallPiece(Floors, HingeS, FVector2D(0.f, -1.f), Along, Thick, WallH, 0.f, 0.f, SlopeDeg);
		const float ArchW = 800.f;
		const float ArchH = 700.f;
		auto FaceX = [&](float Sign, bool bArch, FName Door)
		{
			const FVector2D OutXY(Sign, 0.f);
			const FVector Hinge = Origin + FVector(Sign * Half, 0.f, 0.f);
			const float Rad = FMath::DegreesToRadians(SlopeDeg);
			const FVector WallUp(OutXY.X * FMath::Sin(Rad), OutXY.Y * FMath::Sin(Rad), FMath::Cos(Rad));
			const FVector WallOut(OutXY.X * FMath::Cos(Rad), OutXY.Y * FMath::Cos(Rad), -FMath::Sin(Rad));
			const FVector Tangent = FVector::CrossProduct(WallOut, WallUp);
			auto ShiftY = [&](float WorldY)
			{
				return FVector::DotProduct(FVector(0.f, WorldY - Origin.Y, 0.f), Tangent);
			};
			if (bArch)
			{
				const float Side = (R.ChamberM * M - ArchW) * 0.5f + Extra;
				const float Mid = ArchW * 0.5f + Side * 0.5f;
				AddSlopedWallPiece(Floors, Hinge, OutXY, Side, Thick, WallH, ShiftY(Origin.Y + Mid), 0.f, SlopeDeg);
				AddSlopedWallPiece(Floors, Hinge, OutXY, Side, Thick, WallH, ShiftY(Origin.Y - Mid), 0.f, SlopeDeg);
				AddSlopedWallPiece(Floors, Hinge, OutXY, ArchW, Thick, WallH - ArchH, 0.f, ArchH, SlopeDeg);
				if (!Door.IsNone() && Sign > 0.f)
				{
					AddSlopedWallPiece(Floors, Hinge, OutXY, ArchW, Thick, ArchH, 0.f, 0.f, SlopeDeg);
					Floors.RegisterDoor(Door);
				}
			}
			else
			{
				AddSlopedWallPiece(Floors, Hinge, OutXY, Along, Thick, WallH, 0.f, 0.f, SlopeDeg);
			}
		};
		FaceX(-1.f, bWestArch, NAME_None);
		FaceX(1.f, bEastArch, DoorId);
	}

	FVector SpawnOverlookTop(const FRaidObeliskRecipe& R)
	{
		const float M = 100.f;
		const float Half = R.ChamberM * 0.5f * M;
		const float Thick = 80.f;
		const float WallH = R.WallH * M;
		const float Rad = FMath::DegreesToRadians(R.WallSlopeDeg);
		const float OuterTop = Thick * FMath::Cos(Rad) + WallH * FMath::Sin(Rad);
		const float WallWest = -(Half + OuterTop);
		const float Deep = 600.f;
		const float TopZ = WallH;
		return FVector(WallWest - 40.f - Deep * 0.5f, 0.f, TopZ);
	}

	void StampSpawnOverlook(ACLGreyboxFloors& Floors, const FRaidObeliskRecipe& R)
	{
		const FVector Top = SpawnOverlookTop(R);
		Floors.AddPlatform(Top, 6.f, 8.f, 40.f);
	}
}

void CLBuildRaidObelisk(ACLGreyboxFloors& Floors)
{
	FRaidObeliskRecipe R;
	R.Load();
	for (int32 i = 0; i < R.Chambers.Num(); ++i)
	{
		const FRaidChamberSpec& Spec = R.Chambers[i];
		const FVector Origin = ChamberOrigin(R, i);
		const bool bWest = i > 0;
		const bool bEast = i < R.Chambers.Num() - 1;
		StampChamberShell(Floors, R, Origin, bWest, bEast, Spec.Door);
		TArray<FVector> RingLocals;
		FVector MainLocal;
		PlaceRingLocals(Spec, RingLocals, MainLocal);
		for (int32 r = 0; r < Spec.Rings.Num(); ++r)
		{
			const FVector Center = Origin + (RingLocals.IsValidIndex(r) ? RingLocals[r] : FVector::ZeroVector);
			StampOccupyPad(Floors, Center, Spec.Rings[r], false);
		}
		FVector MainC = Origin + MainLocal + FVector(0.f, 0.f, Spec.Main.ElevateCm);
		StampOccupyPad(Floors, MainC, Spec.Main, true);
		if (Spec.Main.ElevateCm > 50.f)
		{
			for (int32 r = 0; r < RingLocals.Num(); ++r)
			{
				const FVector From = Origin + RingLocals[r];
				const FVector Mid = (From + MainC) * 0.5f;
				const FVector Delta = MainC - From;
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
				Floors.StampModule(FName(TEXT("ramp_mid")), FVector(Mid.X, Mid.Y, From.Z + 40.f), FRotator(-26.6f, Yaw, 0.f));
			}
		}
	}
	StampSpawnOverlook(Floors, R);
	UE_LOG(LogCalling, Display, TEXT("Greybox RaidObelisk chambers=%d pads=%d"), R.Chambers.Num(), Floors.NumPlatforms());
}

FVector CLRaidObeliskPlayerStart()
{
	FRaidObeliskRecipe R;
	R.Load();
	const FVector Top = SpawnOverlookTop(R);
	return Top + FVector(-180.f, 0.f, 130.f);
}

void CLStampRaidObeliskMarkers(UWorld* World, ACLGreyboxFloors& Floors)
{
	if (!World)
	{
		return;
	}
	FRaidObeliskRecipe R;
	R.Load();
	ACLTaskMarker::SpawnAt(World, FName(TEXT("spawn_player")), CLRaidObeliskPlayerStart(), 520.f);
	ACLTaskMarker::SpawnAt(World, FName(TEXT("spawn_default")), CLRaidObeliskPlayerStart(), 520.f);
	for (int32 i = 0; i < R.Chambers.Num(); ++i)
	{
		const FRaidChamberSpec& Spec = R.Chambers[i];
		const FVector Origin = ChamberOrigin(R, i);
		TArray<FVector> RingLocals;
		FVector MainLocal;
		PlaceRingLocals(Spec, RingLocals, MainLocal);
		for (int32 r = 0; r < Spec.Rings.Num(); ++r)
		{
			const FVector Center = Origin + (RingLocals.IsValidIndex(r) ? RingLocals[r] : FVector::ZeroVector);
			StampOccupyMarkers(World, Center, Spec.Rings[r], R.OrbitCount);
		}
		const FVector MainC = Origin + MainLocal + FVector(0.f, 0.f, Spec.Main.ElevateCm);
		StampOccupyMarkers(World, MainC, Spec.Main, R.OrbitCount);
		if (!Spec.Portal.IsNone())
		{
			const float Half = R.ChamberM * 0.5f * 100.f;
			const FVector PortalAt = Origin + FVector(0.f, -(Half - 400.f), 40.f);
			ACLTaskMarker::SpawnAt(World, Spec.Portal, PortalAt, 220.f);
		}
		if (!Spec.Door.IsNone())
		{
			const float Half = R.ChamberM * 0.5f * 100.f;
			ACLTaskMarker::SpawnAt(World, Spec.Door, Origin + FVector(Half + 40.f, 0.f, 100.f), 200.f);
		}
	}
	(void)Floors;
}
