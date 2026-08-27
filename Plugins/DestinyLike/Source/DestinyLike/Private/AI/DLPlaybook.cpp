#include "AI/DLPlaybook.h"
#include "Player/DLPlayerCharacter.h"

UDLPlaybookComponent::UDLPlaybookComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDLPlaybookComponent::ApplyIntentToPawn(ADLPlayerCharacter* Char, const FDLAgentIntent& Intent)
{
	if (Char)
	{
		Char->ApplyAgentIntent(Intent);
	}
}

FDLAgentIntent UDLPlaybookComponent::TickPlaybook(float DeltaSeconds, ADLPlayerCharacter* Char)
{
	(void)DeltaSeconds;
	(void)Char;
	return FDLAgentIntent();
}
