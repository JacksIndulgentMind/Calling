#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/CLTypes.h"
#include "CLAIPersonalityData.generated.h"

USTRUCT(BlueprintType)
struct CALLING_API FCLAIPersonalityWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	ECLNavPersonality Nav = ECLNavPersonality::CoverCycler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	ECLEngagementPersonality Engagement = ECLEngagementPersonality::Pusher;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	float PlanningHorizonSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	float Aggression = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	float CoverDiscipline = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|AI")
	float FlankBias = 0.3f;
};

/**
 * Personality table for raid/PVP bots. Prefer intellect variance over HP sponges.
 */
UCLASS(BlueprintType)
class CALLING_API UCLAIPersonalityData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calling|AI")
	TArray<FCLAIPersonalityWeight> Entries;

	UFUNCTION(BlueprintCallable, Category = "Calling|AI")
	FCLAIPersonalityWeight RollPersonality() const;

	static bool LoadDefaultJson(TArray<FCLAIPersonalityWeight>& OutEntries);
	static FCLAIPersonalityWeight RollFromEntries(const TArray<FCLAIPersonalityWeight>& InEntries);
};
