#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/DLAIPersonalityData.h"
#include "DLEncounterDirector.generated.h"

class ADLCombatAIController;

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLEncounterSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	FDLAIPersonalityWeight Personality;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	bool bElite = false;
};

/**
 * Per-chamber RNG: composition, density, spawn points, personalities.
 * Difficulty via intellect / numbers / terrain — not sponge HP.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLEncounterDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLEncounterDirector();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Raid")
	void BuildAndSpawnChamber(int32 ChamberIndex);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Raid")
	void ClearSpawned();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	TObjectPtr<UDLAIPersonalityData> PersonalityTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	TSubclassOf<APawn> GruntPawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	TSubclassOf<APawn> ElitePawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Raid")
	float ArenaHalfExtent = 5000.f; // ~100m

protected:
	TArray<FDLEncounterSpawnRequest> BuildPlan(int32 ChamberIndex) const;
	FDLAIPersonalityWeight RollDefaultPersonality() const;
	APawn* SpawnOne(const FDLEncounterSpawnRequest& Request);

	UPROPERTY()
	TArray<TObjectPtr<APawn>> SpawnedPawns;
};
