#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/DLAgentIntent.h"
#include "DLPlaybook.generated.h"

class ADLPlayerCharacter;

/**
 * Stub. Later playbooks Tick and write FDLAgentIntent; they never talk HTTP.
 * In-game pawns and the Cursor agent share ADLPlayerCharacter::ApplyAgentIntent.
 * Do not attach this until the playbook phase.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLPlaybookComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLPlaybookComponent();

	static void ApplyIntentToPawn(ADLPlayerCharacter* Char, const FDLAgentIntent& Intent);

	/** Empty until named books (peek / hold / collapse mid / cut lane) exist. */
	virtual FDLAgentIntent TickPlaybook(float DeltaSeconds, ADLPlayerCharacter* Char);
};
