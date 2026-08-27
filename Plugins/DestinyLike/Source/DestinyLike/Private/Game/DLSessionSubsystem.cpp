#include "Game/DLSessionSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLLobbySubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FName DLSessionName = NAME_GameSession;
	const FName DLAttrActivity = FName(TEXT("DL_ACTIVITY"));
	const FName DLAttrMap = FName(TEXT("DL_MAP"));
	const FName DLAttrHost = FName(TEXT("DL_HOST"));
	const FName DLAttrSocialPvp = FName(TEXT("DL_SOCIALPVP"));
}

void UDLSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GConfig->GetBool(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("bUseLan"), bUseLan, GGameIni);

	CreateCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UDLSessionSubsystem::HandleCreateSessionComplete);
	FindCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UDLSessionSubsystem::HandleFindSessionsComplete);
	JoinCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UDLSessionSubsystem::HandleJoinSessionComplete);
	DestroyCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UDLSessionSubsystem::HandleDestroySessionComplete);
}

void UDLSessionSubsystem::Deinitialize()
{
	DestroySession();
	Super::Deinitialize();
}

IOnlineSessionPtr UDLSessionSubsystem::GetSessions() const
{
	if (const IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get())
	{
		return Subsystem->GetSessionInterface();
	}
	return nullptr;
}

FString UDLSessionSubsystem::ResolveHostDisplayName() const
{
	if (const UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>())
	{
		const FDLLocalProfile Active = Profiles->GetActiveProfile();
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

bool UDLSessionSubsystem::HostSession(EDLSceneId Activity, int32 MaxPlayers, EDLSocialPvpMode SocialPvpMode, const FString& MapName)
{
	return HostSession(FDLLobbyInvoice::MakeSocial(EDLLobbyAccess::Open, SocialPvpMode, MaxPlayers), MapName);
}

bool UDLSessionSubsystem::HostSession(const FDLLobbyInvoice& Invoice, const FString& MapName)
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		OnSessionEvent.Broadcast(false, TEXT("Online subsystem unavailable (enable OnlineSubsystemNull)."));
		return false;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
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
	Settings.bShouldAdvertise = Invoice.Access == EDLLobbyAccess::Open || Invoice.Access == EDLLobbyAccess::Friends;
	Settings.bUsesPresence = false;
	Settings.bUseLobbiesIfAvailable = false;
	Settings.Set(DLAttrActivity, static_cast<int32>(Invoice.Activity), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(DLAttrMap, MapName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(DLAttrHost, ResolveHostDisplayName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(DLAttrSocialPvp, static_cast<int32>(Invoice.SocialPvpMode), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const bool bOk = Sessions->CreateSession(0, DLSessionName, Settings);
	if (!bOk)
	{
		OnSessionEvent.Broadcast(false, TEXT("CreateSession failed to start."));
	}
	return bOk;
}

void UDLSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
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

void UDLSessionSubsystem::TravelToMapAsListenServer(const FString& MapName)
{
	if (UWorld* World = GetWorld())
	{
		const FString URL = MapName + TEXT("?listen");
		World->ServerTravel(URL, true);
	}
}

bool UDLSessionSubsystem::FindSessions(EDLSceneId ActivityFilter)
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

void UDLSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
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
			FDLLobbyListing Listing;
			Listing.SessionId = FString::FromInt(i);

			int32 ActivityInt = 0;
			Result.Session.SessionSettings.Get(DLAttrActivity, ActivityInt);
			Listing.Activity = static_cast<EDLSceneId>(ActivityInt);

			// If caller filtered to a non-boot activity, skip mismatches.
			if (SearchActivityFilter != EDLSceneId::Boot && Listing.Activity != SearchActivityFilter)
			{
				continue;
			}

			Result.Session.SessionSettings.Get(DLAttrMap, Listing.MapName);
			Result.Session.SessionSettings.Get(DLAttrHost, Listing.HostDisplayName);
			int32 SocialPvpInt = 0;
			Result.Session.SessionSettings.Get(DLAttrSocialPvp, SocialPvpInt);
			Listing.SocialPvpMode = static_cast<EDLSocialPvpMode>(SocialPvpInt);
			Listing.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Listing.CurrentPlayers = Listing.MaxPlayers - Result.Session.NumOpenPublicConnections;
			CachedListings.Add(Listing);
		}
	}

	OnLobbyListUpdated.Broadcast(CachedListings);
	OnSessionEvent.Broadcast(bWasSuccessful, FString::Printf(TEXT("Found %d lobbies."), CachedListings.Num()));
}

bool UDLSessionSubsystem::JoinSessionByIndex(int32 ListingIndex)
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
	return Sessions->JoinSession(0, DLSessionName, SessionSearch->SearchResults[SearchIndex]);
}

void UDLSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
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

void UDLSessionSubsystem::DestroySession()
{
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions.IsValid())
	{
		bIsHosting = false;
		return;
	}

	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	DestroyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(DestroyCompleteDelegate);
	Sessions->DestroySession(DLSessionName);
}

void UDLSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
	}
	bIsHosting = false;
	OnSessionEvent.Broadcast(bWasSuccessful, TEXT("Session destroyed."));
}
