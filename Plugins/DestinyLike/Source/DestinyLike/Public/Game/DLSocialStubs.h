#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DLSocialStubs.generated.h"

/**
 * Vendor stub: interactable, no economy yet. Later: raid/pvp objectives unlock purchases.
 */
UCLASS()
class DESTINYLIKE_API ADLSocialVendor : public AActor
{
	GENERATED_BODY()

public:
	ADLSocialVendor();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Social")
	void Interact(APawn* Interactor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Social")
	FName VendorId = TEXT("general_goods");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Social")
	FString DisplayName = TEXT("Vendor");
};

/** Local multicast chat stub for social spaces (no accounts). */
UCLASS()
class DESTINYLIKE_API ADLSocialChatRelay : public AActor
{
	GENERATED_BODY()

public:
	ADLSocialChatRelay();

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "DestinyLike|Social")
	void ServerBroadcastChat(const FString& SenderName, const FString& Message);

	UFUNCTION(NetMulticast, Reliable, Category = "DestinyLike|Social")
	void MulticastChat(const FString& SenderName, const FString& Message);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLOnChatMessage, const FString&, SenderName, const FString&, Message);

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Social")
	FDLOnChatMessage OnChatMessage;
};
