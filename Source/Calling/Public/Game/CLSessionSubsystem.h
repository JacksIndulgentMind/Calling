#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Core/CLTypes.h"
#include "Game/CLLobbyTypes.h"
#include "CLSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCLOnLobbyListUpdated, const TArray<FCLLobbyListing>&, Listings);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCLOnSessionEvent, bool, bSuccess, const FString&, Message);

/**
 * Offline listen-server lobbies via NULL/LAN Online Subsystem.
 * No online account required. Local profile is identity.
 */
UCLASS()
class CALLING_API UCLSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Session")
	bool HostSession(ECLSceneId Activity, int32 MaxPlayers, ECLSocialPvpMode SocialPvpMode, const FString& MapName);

	bool HostSession(const FCLLobbyInvoice& Invoice, const FString& MapName);

	UFUNCTION(BlueprintCallable, Category = "Calling|Session")
	bool FindSessions(ECLSceneId ActivityFilter);

	UFUNCTION(BlueprintCallable, Category = "Calling|Session")
	bool JoinSessionByIndex(int32 ListingIndex);

	UFUNCTION(BlueprintCallable, Category = "Calling|Session")
	void DestroySession();

	UFUNCTION(BlueprintPure, Category = "Calling|Session")
	TArray<FCLLobbyListing> GetCachedListings() const { return CachedListings; }

	UFUNCTION(BlueprintPure, Category = "Calling|Session")
	bool IsHosting() const { return bIsHosting; }

	UFUNCTION(BlueprintPure, Category = "Calling|Session")
	ECLSceneId GetHostedActivity() const { return HostedActivity; }

	UFUNCTION(BlueprintPure, Category = "Calling|Session")
	ECLSocialPvpMode GetHostedSocialPvpMode() const { return HostedSocialPvpMode; }

	UPROPERTY(BlueprintAssignable, Category = "Calling|Session")
	FCLOnLobbyListUpdated OnLobbyListUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Calling|Session")
	FCLOnSessionEvent OnSessionEvent;

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
	TArray<FCLLobbyListing> CachedListings;

	UPROPERTY()
	bool bIsHosting = false;

	UPROPERTY()
	ECLSceneId HostedActivity = ECLSceneId::Social;

	UPROPERTY()
	ECLSocialPvpMode HostedSocialPvpMode = ECLSocialPvpMode::Optional;

	UPROPERTY()
	FString PendingTravelMap;

	UPROPERTY()
	bool bUseLan = true;

	UPROPERTY()
	ECLSceneId SearchActivityFilter = ECLSceneId::Boot;
};
