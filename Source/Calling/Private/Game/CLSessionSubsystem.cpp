#include "Game/CLSessionSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbySubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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
	if (UWorld* World = GetWorld())
	{
		const FString URL = MapName + TEXT("?listen");
		World->ServerTravel(URL, true);
	}
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
	OnSessionEvent.Broadcast(bWasSuccessful, TEXT("Session destroyed."));
}
