#include "Game/CLSessionSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLLoopbackJoin.h"
#include "Game/CLSceneRouter.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/CLGameStateBase.h"
#include "HAL/PlatformTime.h"

namespace
{
	const FName CLSessionName = NAME_GameSession;
	const FName CLAttrActivity = FName(TEXT("CL_ACTIVITY"));
	const FName CLAttrMap = FName(TEXT("CL_MAP"));
	const FName CLAttrHost = FName(TEXT("CL_HOST"));
	const FName CLAttrSocialPvp = FName(TEXT("CL_SOCIALPVP"));
}

void UCLSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GConfig->GetBool(TEXT("/Script/Calling.CLSessionSettings"), TEXT("bUseLan"), bUseLan, GGameIni);

	CreateCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCLSessionSubsystem::HandleCreateSessionComplete);
	FindCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCLSessionSubsystem::HandleFindSessionsComplete);
	JoinCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCLSessionSubsystem::HandleJoinSessionComplete);
	DestroyCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCLSessionSubsystem::HandleDestroySessionComplete);
}

void UCLSessionSubsystem::Deinitialize()
{
	StopJoinWatch();
	DestroySession();
	Super::Deinitialize();
}

IOnlineSessionPtr UCLSessionSubsystem::GetSessions() const
{
	if (const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		return Subsystem->GetSessionInterface();
	}
	return nullptr;
}

FString UCLSessionSubsystem::ResolveHostDisplayName() const
{
	if (const UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
	{
		const FCLLocalProfile Active = Profiles->GetActiveProfile();
		if (!Active.DisplayName.IsEmpty())
		{
			return Active.DisplayName;
		}
		if (!Active.Character.CharacterName.IsEmpty())
		{
			return Active.Character.CharacterName;
		}
	}
	return TEXT("Host");
}

bool UCLSessionSubsystem::HostSession(ECLSceneId Activity, int32 MaxPlayers, ECLSocialPvpMode SocialPvpMode, const FString& MapName)
{
	return HostSession(FCLLobbyInvoice::MakeSocial(ECLLobbyAccess::Open, SocialPvpMode, MaxPlayers), MapName);
}

bool UCLSessionSubsystem::HostSession(const FCLLobbyInvoice& Invoice, const FString& MapName)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		OnSessionEvent.Broadcast(false, TEXT("Online subsystem unavailable (enable OnlineSubsystemNull)."));
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->SetPendingInvoice(Invoice);
		}
	}

	PendingTravelMap = MapName;
	HostedActivity = Invoice.Activity;
	HostedSocialPvpMode = Invoice.SocialPvpMode;

	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
	CreateCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(CreateCompleteDelegate);

	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = bUseLan;
	Settings.NumPublicConnections = FMath::Max(2, Invoice.MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bShouldAdvertise = Invoice.Access == ECLLobbyAccess::Open || Invoice.Access == ECLLobbyAccess::Friends;
	Settings.bUsesPresence = false;
	Settings.bUseLobbiesIfAvailable = false;
	Settings.Set(CLAttrActivity, static_cast<int32>(Invoice.Activity), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CLAttrMap, MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CLAttrHost, ResolveHostDisplayName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(CLAttrSocialPvp, static_cast<int32>(Invoice.SocialPvpMode), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const bool bOk = Sessions->CreateSession(0, CLSessionName, Settings);
	if (!bOk)
	{
		OnSessionEvent.Broadcast(false, TEXT("CreateSession failed to start."));
	}
	return bOk;
}

void UCLSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
	}

	bIsHosting = bWasSuccessful;
	if (bWasSuccessful)
	{
		TravelToMapAsListenServer(PendingTravelMap);
		OnSessionEvent.Broadcast(true, TEXT("Hosting listen session."));
	}
	else
	{
		OnSessionEvent.Broadcast(false, TEXT("Failed to create session."));
	}
}

void UCLSessionSubsystem::TravelToMapAsListenServer(const FString& MapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FString URL = MapName;
	if (HostedActivity == ECLSceneId::Social && !URL.Contains(TEXT("game="), ESearchCase::IgnoreCase))
	{
		URL += TEXT("?game=/Script/Calling.CLSocialGameMode");
	}
	if (!URL.Contains(TEXT("listen"), ESearchCase::IgnoreCase))
	{
		URL += TEXT("?listen");
	}
	if (HostedActivity == ECLSceneId::Social)
	{
		FString MapOnly = MapName;
		int32 Q = INDEX_NONE;
		if (MapOnly.FindChar(TEXT('?'), Q))
		{
			MapOnly.LeftInline(Q);
		}
		FString Options;
		if (URL.Split(TEXT("?"), nullptr, &Options))
		{
			UGameplayStatics::OpenLevel(World, FName(*MapOnly), true, Options);
			return;
		}
	}
	World->ServerTravel(URL, true);
}

bool UCLSessionSubsystem::StartComposerLoopbackHost()
{
	UGameInstance* GI = GetGameInstance();
	UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	UCLSceneRouter* Router = GI ? GI->GetSubsystem<UCLSceneRouter>() : nullptr;
	if (!Lobby || !Router)
	{
		OnSessionEvent.Broadcast(false, TEXT("no_lobby"));
		return false;
	}
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			OnSessionEvent.Broadcast(false, TEXT("already_client"));
			return false;
		}
		if (World->GetNetMode() == NM_ListenServer)
		{
			CLLoopbackJoin::WriteBeacon(World);
			OnSessionEvent.Broadcast(true, TEXT("Already listen; beacon refreshed."));
			return true;
		}
	}
	int32 MaxPlayers = 8;
	GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
	Lobby->SetPendingInvoice(FCLLobbyInvoice::MakeComposerPvp(2, MaxPlayers));
	Lobby->ClaimLocalHost();
	const FString Map = Router->GetMapNameForScene(ECLSceneId::Composer);
	PendingTravelMap = FString::Printf(TEXT("%s?game=/Script/Calling.CLComposerGameMode"), *Map);
	HostedActivity = ECLSceneId::Composer;
	bIsHosting = true;
	CLLoopbackJoin::AppendLog(TEXT("virtual host composer listen"));
	TravelToMapAsListenServer(PendingTravelMap);
	OnSessionEvent.Broadcast(true, TEXT("Loopback composer listen."));
	return true;
}

bool UCLSessionSubsystem::JoinLoopback(const FString& Selected)
{
	UWorld* World = GetWorld();
	if (World && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer))
	{
		OnSessionEvent.Broadcast(false, TEXT("already_host"));
		return false;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		OnSessionEvent.Broadcast(false, TEXT("no_pc"));
		return false;
	}
	const FString Connect = CLLoopbackJoin::ResolveConnect(Selected);
	bJoinReadyPending = true;
	CLLoopbackJoin::AppendLog(FString::Printf(TEXT("virtual join %s"), *Connect));
	PC->ClientTravel(Connect, TRAVEL_Absolute);
	OnSessionEvent.Broadcast(true, FString::Printf(TEXT("Joining %s"), *Connect));
	return true;
}

bool UCLSessionSubsystem::FindSessions(ECLSceneId ActivityFilter)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		OnSessionEvent.Broadcast(false, TEXT("Online subsystem unavailable."));
		return false;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->bIsLanQuery = bUseLan;
	SessionSearch->MaxSearchResults = 32;

	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
	FindCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FindCompleteDelegate);

	const bool bOk = Sessions->FindSessions(0, SessionSearch.ToSharedRef());
	if (!bOk)
	{
		OnSessionEvent.Broadcast(false, TEXT("FindSessions failed to start."));
	}

	// Activity filter applied when results arrive (NULL OSS may ignore query settings).
	SearchActivityFilter = ActivityFilter;
	return bOk;
}

void UCLSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
	}

	CachedListings.Reset();
	if (bWasSuccessful && SessionSearch.IsValid())
	{
		for (int32 i = 0; i < SessionSearch->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& Result = SessionSearch->SearchResults[i];
			FCLLobbyListing Listing;
			Listing.SessionId = FString::FromInt(i);

			int32 ActivityInt = 0;
			Result.Session.SessionSettings.Get(CLAttrActivity, ActivityInt);
			Listing.Activity = static_cast<ECLSceneId>(ActivityInt);

			// If caller filtered to a non-boot activity, skip mismatches.
			if (SearchActivityFilter != ECLSceneId::Boot && Listing.Activity != SearchActivityFilter)
			{
				continue;
			}

			Result.Session.SessionSettings.Get(CLAttrMap, Listing.MapName);
			Result.Session.SessionSettings.Get(CLAttrHost, Listing.HostDisplayName);
			int32 SocialPvpInt = 0;
			Result.Session.SessionSettings.Get(CLAttrSocialPvp, SocialPvpInt);
			Listing.SocialPvpMode = static_cast<ECLSocialPvpMode>(SocialPvpInt);
			Listing.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Listing.CurrentPlayers = Listing.MaxPlayers - Result.Session.NumOpenPublicConnections;
			CachedListings.Add(Listing);
		}
	}

	OnLobbyListUpdated.Broadcast(CachedListings);
	OnSessionEvent.Broadcast(bWasSuccessful, FString::Printf(TEXT("Found %d lobbies."), CachedListings.Num()));
}

bool UCLSessionSubsystem::JoinSessionByIndex(int32 ListingIndex)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid() || !SessionSearch.IsValid())
	{
		OnSessionEvent.Broadcast(false, TEXT("No search results to join."));
		return false;
	}
	if (!CachedListings.IsValidIndex(ListingIndex))
	{
		OnSessionEvent.Broadcast(false, TEXT("Invalid lobby index."));
		return false;
	}

	const int32 SearchIndex = FCString::Atoi(*CachedListings[ListingIndex].SessionId);
	if (!SessionSearch->SearchResults.IsValidIndex(SearchIndex))
	{
		OnSessionEvent.Broadcast(false, TEXT("Search result missing."));
		return false;
	}

	Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
	JoinCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(JoinCompleteDelegate);
	return Sessions->JoinSession(0, CLSessionName, SessionSearch->SearchResults[SearchIndex]);
}

void UCLSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);

		if (Result == EOnJoinSessionCompleteResult::Success)
		{
			FString ConnectString;
			if (Sessions->GetResolvedConnectString(SessionName, ConnectString))
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
				{
					PC->ClientTravel(ConnectString, TRAVEL_Absolute);
					OnSessionEvent.Broadcast(true, TEXT("Joining session."));
					return;
				}
			}
		}
	}
	OnSessionEvent.Broadcast(false, TEXT("Join session failed."));
}

void UCLSessionSubsystem::DestroySession()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		bIsHosting = false;
		return;
	}

	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	DestroyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(DestroyCompleteDelegate);
	Sessions->DestroySession(CLSessionName);
}

void UCLSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	}
	bIsHosting = false;
	CLLoopbackJoin::ClearBeacon();
	OnSessionEvent.Broadcast(bWasSuccessful, TEXT("Session destroyed."));
}

int32 UCLSessionSubsystem::SocialMaxPlayers() const
{
	int32 MaxPlayers = 16;
	GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Social"), MaxPlayers, GGameIni);
	return FMath::Max(1, MaxPlayers);
}

bool UCLSessionSubsystem::IsSocialListening() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetNetMode() == NM_ListenServer && HostedActivity == ECLSceneId::Social;
	}
	return false;
}

void UCLSessionSubsystem::LeaveSocialListen()
{
	bJoinPending = false;
	DestroySession();
	bIsHosting = false;
	LiveSocialKind = ECLSocialDefaultKind::Private;
}

void UCLSessionSubsystem::HostPrivateSocial()
{
	LeaveSocialListen();
	LiveSocialKind = ECLSocialDefaultKind::Private;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->SetPendingInvoice(FCLLobbyInvoice::MakeSocial(ECLLobbyAccess::Closed, ECLSocialPvpMode::Optional, SocialMaxPlayers()));
		}
		if (UCLSceneRouter* Router = GI->GetSubsystem<UCLSceneRouter>())
		{
			Router->TravelToScene(ECLSceneId::Social, 0, false);
		}
	}
}

bool UCLSessionSubsystem::HostSocialAudience(ECLSocialDefaultKind Kind)
{
	if (Kind == ECLSocialDefaultKind::Join)
	{
		OnSessionEvent.Broadcast(false, TEXT("join_is_not_host"));
		return false;
	}
	if (Kind == ECLSocialDefaultKind::Private)
	{
		HostPrivateSocial();
		OnSessionEvent.Broadcast(true, TEXT("Private social."));
		return true;
	}

	LeaveSocialListen();
	LiveSocialKind = Kind;
	const ECLLobbyAccess Access = FCLSocialDefault::AccessForKind(Kind);
	UGameInstance* GI = GetGameInstance();
	UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	UCLSceneRouter* Router = GI ? GI->GetSubsystem<UCLSceneRouter>() : nullptr;
	if (!Lobby || !Router)
	{
		OnSessionEvent.Broadcast(false, TEXT("no_lobby"));
		return false;
	}
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			OnSessionEvent.Broadcast(false, TEXT("already_client"));
			return false;
		}
	}
	Lobby->SetPendingInvoice(FCLLobbyInvoice::MakeSocial(Access, ECLSocialPvpMode::Optional, SocialMaxPlayers()));
	const FString Map = Router->GetMapNameForScene(ECLSceneId::Social);
	PendingTravelMap = FString::Printf(TEXT("%s?game=/Script/Calling.CLSocialGameMode"), *Map);
	HostedActivity = ECLSceneId::Social;
	bIsHosting = true;
	CLLoopbackJoin::AppendLog(FString::Printf(TEXT("social host %s"), *FCLSocialDefault::KindToString(Kind)));
	TravelToMapAsListenServer(PendingTravelMap);
	OnSessionEvent.Broadcast(true, TEXT("Social listen."));
	return true;
}

bool UCLSessionSubsystem::JoinSocialHost(const FString& Host, int32 Port)
{
	UWorld* World = GetWorld();
	if (World && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer))
	{
		OnSessionEvent.Broadcast(false, TEXT("already_host"));
		return false;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		OnSessionEvent.Broadcast(false, TEXT("no_pc"));
		return false;
	}
	const FString UseHost = Host.IsEmpty() ? TEXT("127.0.0.1") : Host;
	const int32 UsePort = Port > 0 ? Port : 7777;
	const FString Connect = FString::Printf(TEXT("%s:%d"), *UseHost, UsePort);
	LeaveSocialListen();
	PendingJoinHost = UseHost;
	PendingJoinPort = UsePort;
	LiveSocialKind = ECLSocialDefaultKind::Join;
	bJoinPending = true;
	StartJoinWatch();
	CLLoopbackJoin::AppendLog(FString::Printf(TEXT("social join %s"), *Connect));
	PC->ClientTravel(Connect, TRAVEL_Absolute);
	OnSessionEvent.Broadcast(true, FString::Printf(TEXT("Joining %s"), *Connect));
	return true;
}

void UCLSessionSubsystem::StartJoinWatch()
{
	StopJoinWatch();
	JoinWatchStartSeconds = FPlatformTime::Seconds();
	JoinWatchTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UCLSessionSubsystem::TickJoinWatch),
		0.25f);
}

void UCLSessionSubsystem::StopJoinWatch()
{
	if (JoinWatchTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(JoinWatchTicker);
		JoinWatchTicker.Reset();
	}
}

bool UCLSessionSubsystem::TickJoinWatch(float DeltaTime)
{
	(void)DeltaTime;
	if (!bJoinPending)
	{
		return false;
	}
	if (HasJoinedPendingHost())
	{
		bJoinPending = false;
		LiveSocialKind = ECLSocialDefaultKind::Join;
		return false;
	}
	if (FPlatformTime::Seconds() - JoinWatchStartSeconds >= 8.0)
	{
		NotifyJoinFailed(TEXT("timeout"));
		return false;
	}
	return true;
}

bool UCLSessionSubsystem::HasJoinedPendingHost() const
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client || !World->HasBegunPlay())
	{
		return false;
	}
	const ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>();
	if (!GS || GS->GetSceneId() != ECLSceneId::Social)
	{
		return false;
	}
	const int32 Port = World->URL.Port > 0 ? World->URL.Port : 7777;
	if (Port != PendingJoinPort)
	{
		return false;
	}
	if (World->URL.Host.IsEmpty())
	{
		return true;
	}
	return World->URL.Host.Equals(PendingJoinHost, ESearchCase::IgnoreCase);
}

void UCLSessionSubsystem::NotifyJoinFailed(const FString& Reason)
{
	if (!bJoinPending)
	{
		return;
	}
	bJoinPending = false;
	ApplyJoinFallback();
	(void)Reason;
}

void UCLSessionSubsystem::ApplyJoinFallback()
{
	FCLSocialDefault Def;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			Def = Profiles->GetSocialDefault();
		}
	}
	const FString FallbackKind = FCLSocialDefault::FallbackToString(Def.JoinFallback);
	JoinUnavailable = FString::Printf(TEXT("join unavailable, sending to %s"), *FallbackKind);
	OnSessionEvent.Broadcast(false, JoinUnavailable);
	if (Def.JoinFallback == ECLSocialJoinFallback::Public)
	{
		HostSocialAudience(ECLSocialDefaultKind::Public);
	}
	else
	{
		HostPrivateSocial();
	}
}

void UCLSessionSubsystem::ConsumeJoinUnavailableEvent()
{
	if (JoinUnavailable.IsEmpty())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
		{
			FCLMatchEvent E;
			E.Code = TEXT("join_unavailable");
			E.Detail = JoinUnavailable;
			E.Time = World->GetTimeSeconds();
			GS->AppendMatchEvent(E);
		}
	}
	JoinUnavailable.Empty();
}

bool UCLSessionSubsystem::SaveSocialDefault(ECLSocialDefaultKind Kind, const FString& JoinHost, int32 JoinPort, ECLSocialJoinFallback Fallback)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			FCLSocialDefault Def = Profiles->GetSocialDefault();
			Def.Kind = Kind;
			if (!JoinHost.IsEmpty())
			{
				Def.JoinHost = JoinHost;
			}
			if (JoinPort > 0)
			{
				Def.JoinPort = JoinPort;
			}
			Def.JoinFallback = Fallback;
			return Profiles->SetSocialDefault(Def);
		}
	}
	return false;
}

void UCLSessionSubsystem::ApplySocialDefault()
{
	FCLSocialDefault Def;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			Def = Profiles->GetSocialDefault();
		}
	}
	if (Def.Kind == ECLSocialDefaultKind::Join)
	{
		if (!JoinSocialHost(Def.JoinHost, Def.JoinPort))
		{
			NotifyJoinFailed(TEXT("join_start"));
		}
		return;
	}
	HostSocialAudience(Def.Kind);
}
