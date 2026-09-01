#include "Game/CLLobbyTypes.h"

ECLSocialDefaultKind FCLSocialDefault::KindFromString(const FString& S)
{
	if (S.Equals(TEXT("public"), ESearchCase::IgnoreCase)) return ECLSocialDefaultKind::Public;
	if (S.Equals(TEXT("friends"), ESearchCase::IgnoreCase)) return ECLSocialDefaultKind::Friends;
	if (S.Equals(TEXT("party"), ESearchCase::IgnoreCase)) return ECLSocialDefaultKind::Party;
	if (S.Equals(TEXT("join"), ESearchCase::IgnoreCase)) return ECLSocialDefaultKind::Join;
	return ECLSocialDefaultKind::Private;
}

FString FCLSocialDefault::KindToString(ECLSocialDefaultKind Kind)
{
	switch (Kind)
	{
	case ECLSocialDefaultKind::Public: return TEXT("public");
	case ECLSocialDefaultKind::Friends: return TEXT("friends");
	case ECLSocialDefaultKind::Party: return TEXT("party");
	case ECLSocialDefaultKind::Join: return TEXT("join");
	default: return TEXT("private");
	}
}

ECLSocialJoinFallback FCLSocialDefault::FallbackFromString(const FString& S)
{
	return S.Equals(TEXT("public"), ESearchCase::IgnoreCase)
		? ECLSocialJoinFallback::Public : ECLSocialJoinFallback::Private;
}

FString FCLSocialDefault::FallbackToString(ECLSocialJoinFallback Fallback)
{
	return Fallback == ECLSocialJoinFallback::Public ? TEXT("public") : TEXT("private");
}

ECLLobbyAccess FCLSocialDefault::AccessForKind(ECLSocialDefaultKind Kind)
{
	switch (Kind)
	{
	case ECLSocialDefaultKind::Public: return ECLLobbyAccess::Open;
	case ECLSocialDefaultKind::Friends: return ECLLobbyAccess::Friends;
	case ECLSocialDefaultKind::Party: return ECLLobbyAccess::Party;
	default: return ECLLobbyAccess::Closed;
	}
}

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
	Invoice.GameModeId = FName(TEXT("obelisk_raid"));
	Invoice.MinPlayers = FMath::Max(1, MinPlayers);
	Invoice.MaxPlayers = FMath::Max(Invoice.MinPlayers, MaxPlayers);
	Invoice.Access = ECLLobbyAccess::Closed;
	Invoice.RaidChamberIndex = FMath::Max(0, ChamberIndex);
	return Invoice;
}
