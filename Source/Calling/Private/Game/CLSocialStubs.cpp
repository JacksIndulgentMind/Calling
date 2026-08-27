#include "Game/CLSocialStubs.h"
#include "Core/CLLog.h"

ACLSocialVendor::ACLSocialVendor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ACLSocialVendor::Interact(APawn* Interactor)
{
	UE_LOG(LogCalling, Log, TEXT("Calling: %s interacted with vendor %s (economy stub)"),
		Interactor ? *Interactor->GetName() : TEXT("None"), *VendorId.ToString());
}

ACLSocialChatRelay::ACLSocialChatRelay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

bool ACLSocialChatRelay::ServerBroadcastChat_Validate(const FString& SenderName, const FString& Message)
{
	return Message.Len() > 0 && Message.Len() < 256 && SenderName.Len() < 64;
}

void ACLSocialChatRelay::ServerBroadcastChat_Implementation(const FString& SenderName, const FString& Message)
{
	MulticastChat(SenderName, Message);
}

void ACLSocialChatRelay::MulticastChat_Implementation(const FString& SenderName, const FString& Message)
{
	OnChatMessage.Broadcast(SenderName, Message);
	UE_LOG(LogCalling, Log, TEXT("[SocialChat] %s: %s"), *SenderName, *Message);
}
