#include "AI/CLPlaybook.h"
#include "Player/CLPlayerCharacter.h"

UCLPlaybookComponent::UCLPlaybookComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCLPlaybookComponent::ApplyIntentToPawn(ACLPlayerCharacter* Char, const FCLAgentIntent& Intent)
{
	if (Char)
	{
		Char->ApplyAgentIntent(Intent);
	}
}

FCLAgentIntent UCLPlaybookComponent::TickPlaybook(float DeltaSeconds, ACLPlayerCharacter* Char)
{
	(void)DeltaSeconds;
	(void)Char;
	return FCLAgentIntent();
}
