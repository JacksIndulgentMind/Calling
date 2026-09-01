#pragma once

#include "CoreMinimal.h"
#include "Core/CLTypes.h"
#include "Dom/JsonObject.h"

class UGameInstance;
class ACLPlayerCharacter;
class APlayerController;
class UCLRemoteAgentSeatMotor;

struct CALLING_API FCLAgentStateSerializer
{
	static TSharedRef<FJsonObject> Build(
		UGameInstance* GI,
		ACLPlayerCharacter* Char,
		APlayerController* LocalPC,
		const UCLRemoteAgentSeatMotor* Remote,
		const FGuid& AgentSeatId,
		const FGuid& ProbeSeat);

	/** GameMode on listen/standalone; GameState on NM_Client (no auth GameMode). */
	static ECLSceneId ResolveScene(UGameInstance* GI);

	static void FillSceneMenu(TSharedRef<FJsonObject> Root, UGameInstance* GI, APlayerController* LocalPC);
};
