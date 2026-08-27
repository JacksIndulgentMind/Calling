#include "AI/DLSeatController.h"

ADLSeatController::ADLSeatController()
{
	bWantsPlayerState = false;
	bSetControlRotationFromPawnOrientation = false;
}

void ADLSeatController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	(void)DeltaTime;
	(void)bUpdatePawn;
}
