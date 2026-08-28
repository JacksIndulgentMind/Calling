#include "Nav/CLStrainLimits.h"
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
	FString ConfigPath()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Strain/AugmentedHumanoid.json"));
	}

	FCLStrainLimits LoadOrDefault()
	{
		FCLStrainLimits Limits;
		FString Content;
		if (!FFileHelper::LoadFileToString(Content, *ConfigPath()))
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: missing AugmentedHumanoid.json; using compiled defaults"));
			UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("strain_missing"),
				TEXT("AugmentedHumanoid.json missing; using compiled defaults")));
			return Limits;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: AugmentedHumanoid.json failed to parse; using compiled defaults"));
			UCLErrorBoundary::ReportStatic(nullptr, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("strain_parse"),
				TEXT("AugmentedHumanoid.json failed to parse; using compiled defaults")));
			return Limits;
		}

		if (Root->HasField(TEXT("maxFallBeforeCriticalCm")))
		{
			Limits.MaxFallBeforeCriticalCm = static_cast<float>(Root->GetNumberField(TEXT("maxFallBeforeCriticalCm")));
		}
		return Limits;
	}
}

const FCLStrainLimits& CLStrainLimits::Get()
{
	static const FCLStrainLimits Cached = LoadOrDefault();
	return Cached;
}
