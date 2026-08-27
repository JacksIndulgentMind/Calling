#include "Game/CLInvoiceService.h"
#include "Game/CLLobbyTypes.h"
#include "Misc/ConfigCacheIni.h"

void UCLInvoiceService::SetPending(const FCLLobbyInvoice& Invoice)
{
	Pending = NewObject<UCLInvoiceBox>(this);
	Pending->Value = Invoice;
}

void UCLInvoiceService::ClearPending()
{
	Pending = nullptr;
}

void UCLInvoiceService::ClearLive()
{
	Live = nullptr;
}

void UCLInvoiceService::AdoptPending()
{
	if (Pending)
	{
		Live = Pending;
		Pending = nullptr;
	}
}

void UCLInvoiceService::SetLiveActivity(ECLSceneId Scene)
{
	if (Live)
	{
		Live->Value.Activity = Scene;
	}
}

const FCLLobbyInvoice* UCLInvoiceService::GetPending() const
{
	return Pending ? &Pending->Value : nullptr;
}

const FCLLobbyInvoice* UCLInvoiceService::GetLive() const
{
	return Live ? &Live->Value : nullptr;
}

FName UCLInvoiceService::GetLootRealmId() const
{
	if (const FCLLobbyInvoice* Inv = GetLive())
	{
		return Inv->LootRealm.RealmId.IsNone() ? FName(TEXT("local")) : Inv->LootRealm.RealmId;
	}
	return FName(TEXT("local"));
}

void UCLInvoiceService::ConsumePendingOrDefault(ECLSceneId Scene)
{
	if (Pending)
	{
		Live = Pending;
		Pending = nullptr;
		return;
	}
	int32 MaxPlayers = 8;
	if (Scene == ECLSceneId::Pvp)
	{
		GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
		Live = NewObject<UCLInvoiceBox>(this);
		Live->Value = FCLLobbyInvoice::MakePvp(1, MaxPlayers);
	}
	else if (Scene == ECLSceneId::Composer)
	{
		GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
		Live = NewObject<UCLInvoiceBox>(this);
		Live->Value = FCLLobbyInvoice::MakeComposerPvp(2, MaxPlayers);
	}
	else if (Scene == ECLSceneId::Raid)
	{
		GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Raid"), MaxPlayers, GGameIni);
		Live = NewObject<UCLInvoiceBox>(this);
		Live->Value = FCLLobbyInvoice::MakeRaid(0, 1, MaxPlayers);
	}
}
