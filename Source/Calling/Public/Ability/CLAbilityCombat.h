#pragma once

#include "CoreMinimal.h"

class APawn;
class AActor;
class UWorld;

namespace CLAbilityCombat
{
	AActor* FindNearestHostile(APawn* From, float MaxRange);
	float ApplyDamageToActor(AActor* Target, APawn* Instigator, float Damage);
	void ApplyDamageInRadius(UWorld* World, APawn* Instigator, const FVector& Origin, float Radius, float Damage);
	bool TraceForward(APawn* Owner, float Distance, FHitResult& OutHit);
}
