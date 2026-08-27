#include "AI/CLAIPersonalityData.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace
{
	ECLNavPersonality NavFromString(const FString& S)
	{
		if (S.Equals(TEXT("Wanderer"), ESearchCase::IgnoreCase)) return ECLNavPersonality::Wanderer;
		if (S.Equals(TEXT("Flanker"), ESearchCase::IgnoreCase)) return ECLNavPersonality::Flanker;
		if (S.Equals(TEXT("HoldGround"), ESearchCase::IgnoreCase)) return ECLNavPersonality::HoldGround;
		if (S.Equals(TEXT("AggressivePush"), ESearchCase::IgnoreCase)) return ECLNavPersonality::AggressivePush;
		if (S.Equals(TEXT("CircleConfused"), ESearchCase::IgnoreCase)) return ECLNavPersonality::CircleConfused;
		return ECLNavPersonality::CoverCycler;
	}

	ECLEngagementPersonality EngageFromString(const FString& S)
	{
		if (S.Equals(TEXT("Flanker"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::Flanker;
		if (S.Equals(TEXT("Sniper"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::Sniper;
		if (S.Equals(TEXT("Grenadier"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::Grenadier;
		if (S.Equals(TEXT("Ambusher"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::Ambusher;
		if (S.Equals(TEXT("CeilingShooter"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::CeilingShooter;
		if (S.Equals(TEXT("WeaponThrower"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::WeaponThrower;
		if (S.Equals(TEXT("IdleTroll"), ESearchCase::IgnoreCase)) return ECLEngagementPersonality::IdleTroll;
		return ECLEngagementPersonality::Pusher;
	}

	FString PersonalitiesPath()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("AI/DefaultPersonalities.json"));
	}
}

FCLAIPersonalityWeight UCLAIPersonalityData::RollFromEntries(const TArray<FCLAIPersonalityWeight>& InEntries)
{
	if (InEntries.Num() == 0)
	{
		FCLAIPersonalityWeight Fallback;
		Fallback.Nav = ECLNavPersonality::CoverCycler;
		Fallback.Engagement = ECLEngagementPersonality::Pusher;
		Fallback.Weight = 1.f;
		return Fallback;
	}

	float Total = 0.f;
	for (const FCLAIPersonalityWeight& E : InEntries)
	{
		Total += FMath::Max(0.01f, E.Weight);
	}

	float Pick = FMath::FRandRange(0.f, Total);
	for (const FCLAIPersonalityWeight& E : InEntries)
	{
		Pick -= FMath::Max(0.01f, E.Weight);
		if (Pick <= 0.f)
		{
			return E;
		}
	}
	return InEntries.Last();
}

FCLAIPersonalityWeight UCLAIPersonalityData::RollPersonality() const
{
	return RollFromEntries(Entries);
}

bool UCLAIPersonalityData::LoadDefaultJson(TArray<FCLAIPersonalityWeight>& OutEntries)
{
	OutEntries.Reset();
	FString Text;
	const FString Path = PersonalitiesPath();
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: missing DefaultPersonalities.json at %s"), *Path);
		UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("ai_personalities_missing"),
			FString::Printf(TEXT("Missing DefaultPersonalities.json at %s"), *Path)));
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: failed to parse DefaultPersonalities.json"));
		UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("ai_personalities_parse"),
			TEXT("Failed to parse DefaultPersonalities.json")));
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
			UE_LOG(LogCalling, Warning, TEXT("Calling: personality entry is not an object"));
			continue;
		}
		FCLAIPersonalityWeight W;
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
