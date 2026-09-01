#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UGameInstance;
class ACLPlayerController;

struct CALLING_API FCLDirectorCommandRegistry
{
	static TSharedRef<FJsonObject> Dispatch(
		UGameInstance* GI,
		ACLPlayerController* PC,
		const FString& Action,
		FGuid* AgentSeatId,
		const TSharedPtr<FJsonObject>& Args = nullptr);
};
