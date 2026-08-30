#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CLTaskMarker.generated.h"

/** Map-agnostic nav target. Durable BotBooks goto by Id, not centimeters. */
UCLASS()
class CALLING_API ACLTaskMarker : public AActor
{
	GENERATED_BODY()

public:
	ACLTaskMarker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|BotBook")
	FName Id = NAME_None;

	/** Optional arrival band (cm). 0 = point marker (goto uses verb distXY). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|BotBook")
	float ZoneRadiusCm = 0.f;

	/** spawn.player.*, space.shrine, space.center, spawn.npc, … A marker may have several. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|BotBook")
	TArray<FName> ObjectiveTags;

	bool HasObjectiveTag(FName Tag) const { return ObjectiveTags.Contains(Tag); }

	static ACLTaskMarker* SpawnAt(UWorld* World, FName MarkerId, const FVector& Location, float ZoneRadiusCm = 0.f);
	static ACLTaskMarker* FindById(UWorld* World, FName MarkerId);
	static ACLTaskMarker* FindByTag(UWorld* World, FName Tag);
	static void CollectByTag(UWorld* World, FName Tag, TArray<ACLTaskMarker*>& Out);
	static void DestroyAllInWorld(UWorld* World);
};
