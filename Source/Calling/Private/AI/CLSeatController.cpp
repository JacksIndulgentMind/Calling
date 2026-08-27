#include "AI/CLSeatController.h"

ACLSeatController::ACLSeatController()
{
	bWantsPlayerState = false;
	bSetControlRotationFromPawnOrientation = false;
}

void ACLSeatController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	(void)DeltaTime;
	(void)bUpdatePawn;
}
