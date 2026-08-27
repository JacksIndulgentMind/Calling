#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CLSeatController.generated.h"

/** Controller for a remotely driven combat pawn. No nav personality. */
UCLASS()
class CALLING_API ACLSeatController : public AAIController
{
	GENERATED_BODY()

public:
	ACLSeatController();

	/** Goto / agent intent owns yaw. Default AI copies pawn orientation back onto control rotation. */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
};
