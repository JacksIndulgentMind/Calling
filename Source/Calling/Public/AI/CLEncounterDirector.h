#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/CLAIPersonalityData.h"
#include "CLEncounterDirector.generated.h"

class ACLCombatAIController;

USTRUCT(BlueprintType)
struct CALLING_API FCLEncounterSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	FCLAIPersonalityWeight Personality;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	bool bElite = false;
};

/**
 * Per-chamber RNG: composition, density, spawn points, personalities.
 * Difficulty via intellect / numbers / terrain — not sponge HP.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLEncounterDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLEncounterDirector();

	UFUNCTION(BlueprintCallable, Category = "Calling|Raid")
	void BuildAndSpawnChamber(int32 ChamberIndex);

	UFUNCTION(BlueprintCallable, Category = "Calling|Raid")
	void ClearSpawned();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	TObjectPtr<UCLAIPersonalityData> PersonalityTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	TSubclassOf<APawn> GruntPawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	TSubclassOf<APawn> ElitePawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	float ArenaHalfExtent = 5000.f; // ~100m

protected:
	TArray<FCLEncounterSpawnRequest> BuildPlan(int32 ChamberIndex) const;
	FCLAIPersonalityWeight RollDefaultPersonality() const;
	APawn* SpawnOne(const FCLEncounterSpawnRequest& Request);

	UPROPERTY()
	TArray<TObjectPtr<APawn>> SpawnedPawns;
};
