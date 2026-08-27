#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/DLTypes.h"
#include "DLAIPersonalityData.generated.h"

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLAIPersonalityWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	EDLNavPersonality Nav = EDLNavPersonality::CoverCycler;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	EDLEngagementPersonality Engagement = EDLEngagementPersonality::Pusher;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	float PlanningHorizonSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	float Aggression = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	float CoverDiscipline = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|AI")
	float FlankBias = 0.3f;
};

/**
 * Personality table for raid/PVP bots. Prefer intellect variance over HP sponges.
 */
UCLASS(BlueprintType)
class DESTINYLIKE_API UDLAIPersonalityData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DestinyLike|AI")
	TArray<FDLAIPersonalityWeight> Entries;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|AI")
	FDLAIPersonalityWeight RollPersonality() const;

	static bool LoadDefaultJson(TArray<FDLAIPersonalityWeight>& OutEntries);
	static FDLAIPersonalityWeight RollFromEntries(const TArray<FDLAIPersonalityWeight>& InEntries);
};
