#include "Game/DLParticipantSeat.h"
#include "Player/DLPossessionComponent.h"
#include "GameFramework/Pawn.h"

void UDLParticipantSeat::Configure(const FGuid& InSeatId, const FString& InDisplayName, UDLControllerPlaybook* InPlaybook)
{
	SeatId = InSeatId;
	DisplayName = InDisplayName;
	Playbook = InPlaybook;
}

APawn* UDLParticipantSeat::GetDrivenPawn() const
{
	return Possession ? Possession->GetDrivenPawn() : nullptr;
}
