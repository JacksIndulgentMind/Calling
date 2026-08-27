#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/CLLobbyTypes.h"
#include "CLTravelCoordinator.generated.h"

class UCLInvoiceService;
class UCLSeatRegistry;

UCLASS()
class CALLING_API UCLTravelCoordinator : public UObject
{
	GENERATED_BODY()

public:
	void StampRosterOntoInvoice(UCLInvoiceService* Invoices, UCLSeatRegistry* Seats);
	void RestoreBodiesAfterTravel(UCLInvoiceService* Invoices, UCLSeatRegistry* Seats, const FCLLobbyGate* Gate);
};
