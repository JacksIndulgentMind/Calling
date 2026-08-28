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

	static ACLTaskMarker* SpawnAt(UWorld* World, FName MarkerId, const FVector& Location, float ZoneRadiusCm = 0.f);
	static ACLTaskMarker* FindById(UWorld* World, FName MarkerId);
	static void DestroyAllInWorld(UWorld* World);
};
