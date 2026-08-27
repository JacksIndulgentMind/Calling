#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/CLLobbyTypes.h"
#include "CLInvoiceService.generated.h"

class UCLInvoiceBox;

UCLASS()
class CALLING_API UCLInvoiceService : public UObject
{
	GENERATED_BODY()

public:
	void SetPending(const FCLLobbyInvoice& Invoice);
	void ClearPending();
	void ClearLive();
	void ConsumePendingOrDefault(ECLSceneId Scene);
	void AdoptPending();
	const FCLLobbyInvoice* GetPending() const;
	const FCLLobbyInvoice* GetLive() const;
	UCLInvoiceBox* GetLiveBox() const { return Live; }
	FName GetLootRealmId() const;
	void SetLiveActivity(ECLSceneId Scene);

protected:
	UPROPERTY()
	TObjectPtr<UCLInvoiceBox> Pending;

	UPROPERTY()
	TObjectPtr<UCLInvoiceBox> Live;
};
