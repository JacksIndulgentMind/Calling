#include "Game/DLSocialStubs.h"
#include "Core/DLLog.h"

ADLSocialVendor::ADLSocialVendor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ADLSocialVendor::Interact(APawn* Interactor)
{
	UE_LOG(LogDestinyLike, Log, TEXT("DestinyLike: %s interacted with vendor %s (economy stub)"),
		Interactor ? *Interactor->GetName() : TEXT("None"), *VendorId.ToString());
}

ADLSocialChatRelay::ADLSocialChatRelay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

bool ADLSocialChatRelay::ServerBroadcastChat_Validate(const FString& SenderName, const FString& Message)
{
	return Message.Len() > 0 && Message.Len() < 256 && SenderName.Len() < 64;
}

void ADLSocialChatRelay::ServerBroadcastChat_Implementation(const FString& SenderName, const FString& Message)
{
	MulticastChat(SenderName, Message);
}

void ADLSocialChatRelay::MulticastChat_Implementation(const FString& SenderName, const FString& Message)
{
	OnChatMessage.Broadcast(SenderName, Message);
	UE_LOG(LogDestinyLike, Log, TEXT("[SocialChat] %s: %s"), *SenderName, *Message);
}
