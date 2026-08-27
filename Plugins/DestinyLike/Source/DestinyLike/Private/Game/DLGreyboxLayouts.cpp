#include "Game/DLGreyboxLayouts.h"
#include "Game/DLGreyboxFloors.h"
#include "Game/DLGreyboxFloorPads.inl"
#include "Core/DLLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"

bool FDLPvpThreeLaneRecipe::Load()
{
	FString Path;
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DestinyLike")))
	{
		Path = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/Greybox/PvpThreeLane.json"));
	}
	else
	{
		Path = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("DestinyLike/Config/Greybox/PvpThreeLane.json"));
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogDestinyLike, Warning, TEXT("Greybox: missing PvpThreeLane.json at %s — using baked recipe"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogDestinyLike, Error, TEXT("Greybox: failed to parse PvpThreeLane.json"));
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
	class FSocialLeviathanLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			for (const FDLGeneratedFloorPad& Pad : GSocialLeviathanPads)
			{
				Floors.AddPlatform(FVector(Pad.CenterX, Pad.CenterY, Pad.CenterZ), Pad.SizeXMeters, Pad.SizeYMeters, 40.f);
			}
			UE_LOG(LogDestinyLike, Display, TEXT("Greybox SocialLeviathan pads=%d start=%s"),
				Floors.NumPlatforms(), *Floors.GetPlayerStartLocation().ToCompactString());
		}
	};

	class FSocialSquareLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 100.f, 100.f, 40.f);
			UE_LOG(LogDestinyLike, Display, TEXT("Greybox SocialSquare start=%s"),
				*Floors.GetPlayerStartLocation().ToCompactString());
		}
	};

	class FPvpThreeLaneLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			Floors.BuildPvpThreeLane();
			UE_LOG(LogDestinyLike, Display, TEXT("Greybox PvpThreeLane pads=%d red=%s blue=%s"),
				Floors.NumPlatforms(),
				*Floors.GetPlayerStartLocation().ToCompactString(),
				*Floors.GetBluePlayerStartLocation().ToCompactString());
		}
	};

	class FPvpBannerfallLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
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

	class FRaidKalliLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 50.f, 50.f);
			Floors.AddPlatform(FVector(1200.f, 1200.f, 0.f), 4.f, 4.f);
			Floors.AddPlatform(FVector(-1400.f, 800.f, 0.f), 4.f, 4.f);
			Floors.AddPlatform(FVector(400.f, -1500.f, 0.f), 4.f, 4.f);
		}
	};

	class FRaidShuroLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 80.f, 10.f);
			Floors.AddPlatform(FVector(-1000.f, 1200.f, 0.f), 8.f, 8.f);
			Floors.AddPlatform(FVector(2000.f, -1200.f, 0.f), 8.f, 8.f);
			Floors.AddPlatform(FVector(0.f, 800.f, 0.f), 6.f, 6.f);
		}
	};

	class FRaidMorgethLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 40.f, 40.f);
			Floors.AddPlatform(FVector(1800.f, 1800.f, 0.f), 6.f, 6.f);
			Floors.AddPlatform(FVector(-1800.f, 1800.f, 0.f), 6.f, 6.f);
			Floors.AddPlatform(FVector(1800.f, -1800.f, 0.f), 6.f, 6.f);
			Floors.AddPlatform(FVector(-1800.f, -1800.f, 0.f), 6.f, 6.f);
		}
	};

	class FRaidVaultLayout final : public IDLGreyboxLayout
	{
	public:
		virtual void Build(ADLGreyboxFloors& Floors) override
		{
			Floors.AddPlatform(FVector(0.f, 0.f, 0.f), 20.f, 20.f);
		}
	};
}

TUniquePtr<IDLGreyboxLayout> DLMakeGreyboxLayout(EDLGreyboxLayout Id)
{
	switch (Id)
	{
	case EDLGreyboxLayout::SocialLeviathan: return MakeUnique<FSocialLeviathanLayout>();
	case EDLGreyboxLayout::SocialSquare: return MakeUnique<FSocialSquareLayout>();
	case EDLGreyboxLayout::PvpThreeLane: return MakeUnique<FPvpThreeLaneLayout>();
	case EDLGreyboxLayout::PvpBannerfall: return MakeUnique<FPvpBannerfallLayout>();
	case EDLGreyboxLayout::RaidKalli: return MakeUnique<FRaidKalliLayout>();
	case EDLGreyboxLayout::RaidShuro: return MakeUnique<FRaidShuroLayout>();
	case EDLGreyboxLayout::RaidMorgeth: return MakeUnique<FRaidMorgethLayout>();
	case EDLGreyboxLayout::RaidVault: return MakeUnique<FRaidVaultLayout>();
	default: return MakeUnique<FSocialSquareLayout>();
	}
}
