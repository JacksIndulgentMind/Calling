#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UCLLobbySubsystem;

/**
 * One hub codec. HTTP POST /hub and WebSocket 18766 both call Dispatch.
 * Optional `via` (net-human seat) is routed by the HTTP/WS host before Dispatch.
 * Strategy per `type` (join, subscribe, ready, go, mindcontrol, setteam, appendBotBook, branchBotBook, plan, goto, view).
 */
struct CALLING_API FCLHubCommandRegistry
{
	/** Optional FallbackSeat is updated on successful join. */
	static TSharedRef<FJsonObject> Dispatch(
		UCLLobbySubsystem* Lobby,
		const TSharedPtr<FJsonObject>& Root,
		FGuid* FallbackSeat = nullptr);
};
