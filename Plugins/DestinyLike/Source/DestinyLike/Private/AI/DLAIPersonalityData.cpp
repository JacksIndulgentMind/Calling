#include "AI/DLAIPersonalityData.h"
#include "Core/DLLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"

namespace
{
	EDLNavPersonality NavFromString(const FString& S)
	{
		if (S.Equals(TEXT("Wanderer"), ESearchCase::IgnoreCase)) return EDLNavPersonality::Wanderer;
		if (S.Equals(TEXT("Flanker"), ESearchCase::IgnoreCase)) return EDLNavPersonality::Flanker;
		if (S.Equals(TEXT("HoldGround"), ESearchCase::IgnoreCase)) return EDLNavPersonality::HoldGround;
		if (S.Equals(TEXT("AggressivePush"), ESearchCase::IgnoreCase)) return EDLNavPersonality::AggressivePush;
		if (S.Equals(TEXT("CircleConfused"), ESearchCase::IgnoreCase)) return EDLNavPersonality::CircleConfused;
		return EDLNavPersonality::CoverCycler;
	}

	EDLEngagementPersonality EngageFromString(const FString& S)
	{
		if (S.Equals(TEXT("Flanker"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::Flanker;
		if (S.Equals(TEXT("Sniper"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::Sniper;
		if (S.Equals(TEXT("Grenadier"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::Grenadier;
		if (S.Equals(TEXT("Ambusher"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::Ambusher;
		if (S.Equals(TEXT("CeilingShooter"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::CeilingShooter;
		if (S.Equals(TEXT("WeaponThrower"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::WeaponThrower;
		if (S.Equals(TEXT("IdleTroll"), ESearchCase::IgnoreCase)) return EDLEngagementPersonality::IdleTroll;
		return EDLEngagementPersonality::Pusher;
	}

	FString PersonalitiesPath()
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DestinyLike")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/AI/DefaultPersonalities.json"));
		}
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("DestinyLike/Config/AI/DefaultPersonalities.json"));
	}
}

FDLAIPersonalityWeight UDLAIPersonalityData::RollFromEntries(const TArray<FDLAIPersonalityWeight>& InEntries)
{
	if (InEntries.Num() == 0)
	{
		FDLAIPersonalityWeight Fallback;
		Fallback.Nav = EDLNavPersonality::CoverCycler;
		Fallback.Engagement = EDLEngagementPersonality::Pusher;
		Fallback.Weight = 1.f;
		return Fallback;
	}

	float Total = 0.f;
	for (const FDLAIPersonalityWeight& E : InEntries)
	{
		Total += FMath::Max(0.01f, E.Weight);
	}

	float Pick = FMath::FRandRange(0.f, Total);
	for (const FDLAIPersonalityWeight& E : InEntries)
	{
		Pick -= FMath::Max(0.01f, E.Weight);
		if (Pick <= 0.f)
		{
			return E;
		}
	}
	return InEntries.Last();
}

FDLAIPersonalityWeight UDLAIPersonalityData::RollPersonality() const
{
	return RollFromEntries(Entries);
}

bool UDLAIPersonalityData::LoadDefaultJson(TArray<FDLAIPersonalityWeight>& OutEntries)
{
	OutEntries.Reset();
	FString Text;
	const FString Path = PersonalitiesPath();
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: missing DefaultPersonalities.json at %s"), *Path);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: failed to parse DefaultPersonalities.json"));
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("entries"), List) || !List)
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Val : *List)
	{
		const TSharedPtr<FJsonObject> Obj = Val->AsObject();
		if (!Obj.IsValid())
		{
			UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: personality entry is not an object"));
			continue;
		}
		FDLAIPersonalityWeight W;
		W.Nav = NavFromString(Obj->GetStringField(TEXT("nav")));
		W.Engagement = EngageFromString(Obj->GetStringField(TEXT("engagement")));
		W.Weight = Obj->GetNumberField(TEXT("weight"));
		W.PlanningHorizonSeconds = Obj->GetNumberField(TEXT("planningHorizonSeconds"));
		W.Aggression = Obj->GetNumberField(TEXT("aggression"));
		W.CoverDiscipline = Obj->GetNumberField(TEXT("coverDiscipline"));
		W.FlankBias = Obj->GetNumberField(TEXT("flankBias"));
		OutEntries.Add(W);
	}
	return OutEntries.Num() > 0;
}
