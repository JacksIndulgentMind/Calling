#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UGameInstance;
class ACLPlayerCharacter;
class APlayerController;
class UCLRemoteAgentPlaybook;

struct CALLING_API FCLAgentStateSerializer
{
	static TSharedRef<FJsonObject> Build(
		UGameInstance* GI,
		ACLPlayerCharacter* Char,
		APlayerController* LocalPC,
		const UCLRemoteAgentPlaybook* Remote,
		const FGuid& AgentSeatId,
		const FGuid& ProbeSeat);

	static void FillSceneMenu(TSharedRef<FJsonObject> Root, UGameInstance* GI, APlayerController* LocalPC);
};
