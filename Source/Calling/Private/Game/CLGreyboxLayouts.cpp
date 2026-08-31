#include "Game/CLGreyboxLayouts.h"
#include "Game/CLGreyboxFloors.h"
#include "Game/CLGreyboxFloorPads.inl"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

bool FCLPvpThreeLaneRecipe::Load()
{
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Greybox/PvpThreeLane.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Greybox: missing PvpThreeLane.json at %s — using baked recipe"), *Path);
		UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("pvp_recipe_missing"),
			FString::Printf(TEXT("Missing PvpThreeLane.json at %s"), *Path)));
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogCalling, Error, TEXT("Greybox: failed to parse PvpThreeLane.json"));
		UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("pvp_recipe_parse"),
			TEXT("Failed to parse PvpThreeLane.json")));
		return false;
	}
	PadDepthM = Root->GetNumberField(TEXT("padDepthM"));
	CourtM = Root->GetNumberField(TEXT("courtM"));
	RavineM = Root->GetNumberField(TEXT("ravineM"));
	WidthM = Root->GetNumberField(TEXT("widthM"));
	SlabZ = Root->GetNumberField(TEXT("slabZ"));
	LaneW = Root->GetNumberField(TEXT("laneW"));
	PadInnerM = Root->GetNumberField(TEXT("padInnerM"));
	HalfCoverZ = Root->GetNumberField(TEXT("halfCoverZ"));
	RailStepM = Root->GetNumberField(TEXT("railStepM"));
	auto ReadArr3 = [&](const TCHAR* Key, float Out[3])
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Root->TryGetArrayField(Key, Arr) && Arr && Arr->Num() >= 3)
		{
			Out[0] = (*Arr)[0]->AsNumber();
			Out[1] = (*Arr)[1]->AsNumber();
			Out[2] = (*Arr)[2]->AsNumber();
		}
	};
	ReadArr3(TEXT("laneYMeters"), LaneYMeters);
	ReadArr3(TEXT("laneStepM"), LaneStepM);
	ReadArr3(TEXT("lanePhaseM"), LanePhaseM);
	ReadArr3(TEXT("courtRadiiM"), CourtRadiiM);
	const TArray<TSharedPtr<FJsonValue>>* Cross = nullptr;
	if (Root->TryGetArrayField(TEXT("crossFractions"), Cross) && Cross && Cross->Num() >= 2)
	{
		CrossFractions[0] = (*Cross)[0]->AsNumber();
		CrossFractions[1] = (*Cross)[1]->AsNumber();
	}
	const TArray<TSharedPtr<FJsonValue>>* Rings = nullptr;
	if (Root->TryGetArrayField(TEXT("ringCount"), Rings) && Rings && Rings->Num() >= 3)
	{
		RingCount[0] = static_cast<int32>((*Rings)[0]->AsNumber());
		RingCount[1] = static_cast<int32>((*Rings)[1]->AsNumber());
		RingCount[2] = static_cast<int32>((*Rings)[2]->AsNumber());
	}
	if (Root->HasField(TEXT("menhirRadiusM")))
	{
		MenhirRadiusM = static_cast<float>(Root->GetNumberField(TEXT("menhirRadiusM")));
	}
	if (Root->HasField(TEXT("menhirCount")))
	{
		MenhirCount = static_cast<int32>(Root->GetNumberField(TEXT("menhirCount")));
	}
	if (Root->HasField(TEXT("menhirDepthCm")))
	{
		MenhirDepthCm = static_cast<float>(Root->GetNumberField(TEXT("menhirDepthCm")));
	}
	if (Root->HasField(TEXT("menhirWidthCm")))
	{
		MenhirWidthCm = static_cast<float>(Root->GetNumberField(TEXT("menhirWidthCm")));
	}
	if (Root->HasField(TEXT("menhirHeightCm")))
	{
		MenhirHeightCm = static_cast<float>(Root->GetNumberField(TEXT("menhirHeightCm")));
	}
	MenhirSpanCm = Root->HasField(TEXT("menhirSpanCm"))
		? static_cast<float>(Root->GetNumberField(TEXT("menhirSpanCm"))) : MenhirDepthCm;
	MenhirPostWidthCm = Root->HasField(TEXT("menhirPostWidthCm"))
		? static_cast<float>(Root->GetNumberField(TEXT("menhirPostWidthCm"))) : MenhirWidthCm;
	MenhirPostDepthCm = Root->HasField(TEXT("menhirPostDepthCm"))
		? static_cast<float>(Root->GetNumberField(TEXT("menhirPostDepthCm"))) : MenhirWidthCm;
	if (Root->HasField(TEXT("menhirLintelHeightCm")))
	{
		MenhirLintelHeightCm = static_cast<float>(Root->GetNumberField(TEXT("menhirLintelHeightCm")));
	}
	return true;
}

namespace
{
	class FSocialExtractedLayout final : public ICLGreyboxLayout
	{
	public:
		virtual void Build(ACLGreyboxFloors& Floors) override
		{
			for (const FCLGeneratedFloorPad& Pad : GSocialExtractedPads)
			{
				Floors.AddPlatform(FVector(Pad.CenterX, Pad.CenterY, Pad.CenterZ), Pad.SizeXMeters, Pad.SizeYMeters, 40.f);
			}
			UE_LOG(LogCalling, Display, TEXT("Greybox SocialExtracted pads=%d start=%s"),
				Floors.NumPlatforms(), *Floors.GetPlayerStartLocation().ToCompactString());
		}
	};

	class FSocialSquareLayout final : public ICLGreyboxLayout
	{
	public:
		virtual void Build(ACLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 100.f, 100.f, 40.f);
			UE_LOG(LogCalling, Display, TEXT("Greybox SocialSquare start=%s"),
				*Floors.GetPlayerStartLocation().ToCompactString());
		}
	};

	class FPvpThreeLaneLayout final : public ICLGreyboxLayout
	{
	public:
		virtual void Build(ACLGreyboxFloors& Floors) override
		{
			Floors.BuildPvpThreeLane();
			UE_LOG(LogCalling, Display, TEXT("Greybox PvpThreeLane pads=%d red=%s blue=%s"),
				Floors.NumPlatforms(),
				*Floors.GetPlayerStartLocation().ToCompactString(),
				*Floors.GetBluePlayerStartLocation().ToCompactString());
		}
	};

	class FPvpExtractedLayout final : public ICLGreyboxLayout
	{
	public:
		virtual void Build(ACLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 80.f, 14.f);
			Floors.AddPlatform(FVector(0.f, 2800.f, 0.f), 28.f, 28.f);
			Floors.AddPlatform(FVector(0.f, -2800.f, 0.f), 28.f, 28.f);
			Floors.AddPlatform(FVector(0.f, 4500.f, 0.f), 70.f, 8.f);
			Floors.AddPlatform(FVector(0.f, -4500.f, 0.f), 70.f, 8.f);
			Floors.AddPlatform(FVector(-5000.f, 0.f, 0.f), 12.f, 12.f);
			Floors.AddPlatform(FVector(5000.f, 0.f, 0.f), 12.f, 12.f);
			Floors.AddPlatform(FVector(0.f, 1400.f, 0.f), 8.f, 8.f);
			Floors.AddPlatform(FVector(0.f, -1400.f, 0.f), 8.f, 8.f);
			Floors.AddPlatform(FVector(-4000.f, 0.f, 0.f), 8.f, 8.f);
			Floors.AddPlatform(FVector(4000.f, 0.f, 0.f), 8.f, 8.f);
		}
	};

	class FRaidObeliskLayout final : public ICLGreyboxLayout
	{
	public:
		virtual void Build(ACLGreyboxFloors& Floors) override
		{
			CLBuildRaidObelisk(Floors);
		}
	};

	class FPracticePillarLayout final : public ICLGreyboxLayout
	{
	public:
		virtual void Build(ACLGreyboxFloors& Floors) override
		{
			Floors.BuildPracticePillar();
		}
	};
}

TUniquePtr<ICLGreyboxLayout> CLMakeGreyboxLayout(ECLGreyboxLayout Id)
{
	switch (Id)
	{
	case ECLGreyboxLayout::SocialExtracted: return MakeUnique<FSocialExtractedLayout>();
	case ECLGreyboxLayout::SocialSquare: return MakeUnique<FSocialSquareLayout>();
	case ECLGreyboxLayout::PvpThreeLane: return MakeUnique<FPvpThreeLaneLayout>();
	case ECLGreyboxLayout::PvpExtracted: return MakeUnique<FPvpExtractedLayout>();
	case ECLGreyboxLayout::RaidCourt:
	case ECLGreyboxLayout::RaidApproach:
	case ECLGreyboxLayout::RaidArena:
	case ECLGreyboxLayout::RaidPit:
	case ECLGreyboxLayout::RaidObelisk:
		return MakeUnique<FRaidObeliskLayout>();
	case ECLGreyboxLayout::PracticePillar: return MakeUnique<FPracticePillarLayout>();
	default: return MakeUnique<FSocialSquareLayout>();
	}
}
