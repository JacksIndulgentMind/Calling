#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/CLAgentIntent.h"
#include "CLPlaybook.generated.h"

class ACLPlayerCharacter;

/**
 * Stub. Later playbooks Tick and write FCLAgentIntent; they never talk HTTP.
 * In-game pawns and the Cursor agent share ACLPlayerCharacter::ApplyAgentIntent.
 * Do not attach this until the playbook phase.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLPlaybookComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLPlaybookComponent();

	static void ApplyIntentToPawn(ACLPlayerCharacter* Char, const FCLAgentIntent& Intent);

	/** Empty until named books (peek / hold / collapse mid / cut lane) exist. */
	virtual FCLAgentIntent TickPlaybook(float DeltaSeconds, ACLPlayerCharacter* Char);
};
