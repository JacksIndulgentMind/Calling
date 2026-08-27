#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CLGreyboxRescue.generated.h"

class ACLGreyboxFloors;

/**
 * Void rescue and missing-pawn respawn. Kept off ACLGreyboxFloors so nav rebuild
 * can be a one-shot timer and the floors actor does not need to Tick.
 */
UCLASS()
class CALLING_API UCLGreyboxRescue : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLGreyboxRescue();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void RescueFallenPawns() const;

protected:
	void RespawnMissingPawns(float DeltaSeconds);

	float MissingPawnSeconds = 0.f;
};
