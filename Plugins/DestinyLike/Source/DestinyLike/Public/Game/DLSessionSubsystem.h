#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Core/DLTypes.h"
#include "Game/DLLobbyTypes.h"
#include "DLSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLOnLobbyListUpdated, const TArray<FDLLobbyListing>&, Listings);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLOnSessionEvent, bool, bSuccess, const FString&, Message);

/**
 * Offline listen-server lobbies via NULL/LAN Online Subsystem.
 * No online account required. Local profile is identity.
 */
UCLASS()
class DESTINYLIKE_API UDLSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Session")
	bool HostSession(EDLSceneId Activity, int32 MaxPlayers, EDLSocialPvpMode SocialPvpMode, const FString& MapName);

	bool HostSession(const FDLLobbyInvoice& Invoice, const FString& MapName);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Session")
	bool FindSessions(EDLSceneId ActivityFilter);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Session")
	bool JoinSessionByIndex(int32 ListingIndex);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Session")
	void DestroySession();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Session")
	TArray<FDLLobbyListing> GetCachedListings() const { return CachedListings; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Session")
	bool IsHosting() const { return bIsHosting; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Session")
	EDLSceneId GetHostedActivity() const { return HostedActivity; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Session")
	EDLSocialPvpMode GetHostedSocialPvpMode() const { return HostedSocialPvpMode; }

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Session")
	FDLOnLobbyListUpdated OnLobbyListUpdated;

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Session")
	FDLOnSessionEvent OnSessionEvent;

private:
	IOnlineSessionPtr GetSessions() const;
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void TravelToMapAsListenServer(const FString& MapName);
	FString ResolveHostDisplayName() const;

	FOnCreateSessionCompleteDelegate CreateCompleteDelegate;
	FOnFindSessionsCompleteDelegate FindCompleteDelegate;
	FOnJoinSessionCompleteDelegate JoinCompleteDelegate;
	FOnDestroySessionCompleteDelegate DestroyCompleteDelegate;

	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;

	TSharedPtr<class FOnlineSessionSearch> SessionSearch;

	UPROPERTY()
	TArray<FDLLobbyListing> CachedListings;

	UPROPERTY()
	bool bIsHosting = false;

	UPROPERTY()
	EDLSceneId HostedActivity = EDLSceneId::Social;

	UPROPERTY()
	EDLSocialPvpMode HostedSocialPvpMode = EDLSocialPvpMode::Optional;

	UPROPERTY()
	FString PendingTravelMap;

	UPROPERTY()
	bool bUseLan = true;

	UPROPERTY()
	EDLSceneId SearchActivityFilter = EDLSceneId::Boot;
};
