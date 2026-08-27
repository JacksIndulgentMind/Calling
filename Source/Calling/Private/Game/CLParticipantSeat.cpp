#include "Game/CLParticipantSeat.h"
#include "Player/CLPossessionComponent.h"
#include "GameFramework/Pawn.h"

void UCLParticipantSeat::Configure(const FGuid& InSeatId, const FString& InDisplayName, UCLControllerPlaybook* InPlaybook)
{
	SeatId = InSeatId;
	DisplayName = InDisplayName;
	Playbook = InPlaybook;
}

APawn* UCLParticipantSeat::GetDrivenPawn() const
{
	return Possession ? Possession->GetDrivenPawn() : nullptr;
}
