#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CLHeadlessAgent.generated.h"

class UCLPossessionComponent;

/** Seat anchor with no pawn. Can still mind-control. */
UCLASS()
class CALLING_API ACLHeadlessAgent : public AActor
{
	GENERATED_BODY()

public:
	ACLHeadlessAgent();

	UCLPossessionComponent* GetPossession() const { return Possession; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Calling")
	TObjectPtr<UCLPossessionComponent> Possession;
};
