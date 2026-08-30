#include "Game/CLLobbyTypes.h"

FCLLobbyInvoice FCLLobbyInvoice::MakeSocial(ECLLobbyAccess Access, ECLSocialPvpMode PvpMode, int32 MaxPlayers)
{
	FCLLobbyInvoice Invoice;
	Invoice.Activity = ECLSceneId::Social;
	Invoice.MinPlayers = 1;
	Invoice.MaxPlayers = FMath::Max(1, MaxPlayers);
	Invoice.Access = Access;
	Invoice.SocialPvpMode = PvpMode;
	return Invoice;
}

FCLLobbyInvoice FCLLobbyInvoice::MakeComposerPvp(int32 MinPlayers, int32 MaxPlayers)
{
	FCLLobbyInvoice Invoice;
	Invoice.Activity = ECLSceneId::Composer;
	Invoice.GameModeId = FName(TEXT("shrine_clash"));
	Invoice.MinPlayers = FMath::Max(2, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = ECLLobbyAccess::Open;
	return Invoice;
}

FCLLobbyInvoice FCLLobbyInvoice::MakePvp(int32 MinPlayers, int32 MaxPlayers)
{
	FCLLobbyInvoice Invoice;
	Invoice.Activity = ECLSceneId::Pvp;
	Invoice.GameModeId = FName(TEXT("shrine_clash"));
	Invoice.MinPlayers = FMath::Max(1, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = ECLLobbyAccess::Open;
	return Invoice;
}

FCLLobbyInvoice FCLLobbyInvoice::MakeRaid(int32 ChamberIndex, int32 MinPlayers, int32 MaxPlayers)
{
	FCLLobbyInvoice Invoice;
	Invoice.Activity = ECLSceneId::Raid;
	Invoice.MinPlayers = FMath::Max(1, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = ECLLobbyAccess::Closed;
	Invoice.RaidChamberIndex = FMath::Max(0, ChamberIndex);
	return Invoice;
}
