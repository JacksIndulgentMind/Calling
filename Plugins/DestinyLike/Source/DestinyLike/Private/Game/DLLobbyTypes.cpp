#include "Game/DLLobbyTypes.h"

FDLLobbyInvoice FDLLobbyInvoice::MakeSocial(EDLLobbyAccess Access, EDLSocialPvpMode PvpMode, int32 MaxPlayers)
{
	FDLLobbyInvoice Invoice;
	Invoice.Activity = EDLSceneId::Social;
	Invoice.MinPlayers = 1;
	Invoice.MaxPlayers = FMath::Max(1, MaxPlayers);
	Invoice.Access = Access;
	Invoice.SocialPvpMode = PvpMode;
	return Invoice;
}

FDLLobbyInvoice FDLLobbyInvoice::MakeComposerPvp(int32 MinPlayers, int32 MaxPlayers)
{
	FDLLobbyInvoice Invoice;
	Invoice.Activity = EDLSceneId::Composer;
	Invoice.MinPlayers = FMath::Max(2, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = EDLLobbyAccess::Open;
	return Invoice;
}

FDLLobbyInvoice FDLLobbyInvoice::MakePvp(int32 MinPlayers, int32 MaxPlayers)
{
	FDLLobbyInvoice Invoice;
	Invoice.Activity = EDLSceneId::Pvp;
	Invoice.MinPlayers = FMath::Max(1, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = EDLLobbyAccess::Open;
	return Invoice;
}

FDLLobbyInvoice FDLLobbyInvoice::MakeRaid(int32 ChamberIndex, int32 MinPlayers, int32 MaxPlayers)
{
	FDLLobbyInvoice Invoice;
	Invoice.Activity = EDLSceneId::Raid;
	Invoice.MinPlayers = FMath::Max(1, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = EDLLobbyAccess::Closed;
	Invoice.RaidChamberIndex = FMath::Max(0, ChamberIndex);
	return Invoice;
}
