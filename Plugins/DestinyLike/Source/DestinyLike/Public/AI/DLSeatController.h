#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DLSeatController.generated.h"

/** Controller for a remotely driven combat pawn. No nav personality. */
UCLASS()
class DESTINYLIKE_API ADLSeatController : public AAIController
{
	GENERATED_BODY()

public:
	ADLSeatController();

	/** Goto / agent intent owns yaw. Default AI copies pawn orientation back onto control rotation. */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
};
