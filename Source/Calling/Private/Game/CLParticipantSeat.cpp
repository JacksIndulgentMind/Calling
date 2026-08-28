#include "Game/CLParticipantSeat.h"
#include "Player/CLPossessionComponent.h"
#include "GameFramework/Pawn.h"

void UCLParticipantSeat::Configure(const FGuid& InSeatId, const FString& InDisplayName, UCLSeatMotor* InMotor)
{
	SeatId = InSeatId;
	DisplayName = InDisplayName;
	Motor = InMotor;
}

APawn* UCLParticipantSeat::GetDrivenPawn() const
{
	return Possession ? Possession->GetDrivenPawn() : nullptr;
}
