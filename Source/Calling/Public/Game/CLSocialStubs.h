#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CLSocialStubs.generated.h"

/**
 * Vendor stub: interactable, no economy yet. Later: raid/pvp objectives unlock purchases.
 */
UCLASS()
class CALLING_API ACLSocialVendor : public AActor
{
	GENERATED_BODY()

public:
	ACLSocialVendor();

	UFUNCTION(BlueprintCallable, Category = "Calling|Social")
	void Interact(APawn* Interactor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Social")
	FName VendorId = TEXT("general_goods");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Social")
	FString DisplayName = TEXT("Vendor");
};

/** Local multicast chat stub for social spaces (no accounts). */
UCLASS()
class CALLING_API ACLSocialChatRelay : public AActor
{
	GENERATED_BODY()

public:
	ACLSocialChatRelay();

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Calling|Social")
	void ServerBroadcastChat(const FString& SenderName, const FString& Message);

	UFUNCTION(NetMulticast, Reliable, Category = "Calling|Social")
	void MulticastChat(const FString& SenderName, const FString& Message);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCLOnChatMessage, const FString&, SenderName, const FString&, Message);

	UPROPERTY(BlueprintAssignable, Category = "Calling|Social")
	FCLOnChatMessage OnChatMessage;
};
