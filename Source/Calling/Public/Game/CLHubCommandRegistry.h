#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UCLLobbySubsystem;

/**
 * One hub codec. HTTP POST /hub and WebSocket 18766 both call Dispatch.
 * Optional `connectMode=proxy` on host HTTP/WS forwards to the guest ingress
 * (same Dispatch as guest 18767). Stamp `listenPort`/`recv` before Dispatch.
 * Drive JSON should include `intendedTarget`.
 * Strategy per `type` (join, subscribe, ready, go, mindcontrol, setteam, clearBotBook, appendBotBook, branchBotBook, plan, goto, view).
 */
struct CALLING_API FCLHubCommandRegistry
{
	/** Optional FallbackSeat is updated on successful join. */
	static TSharedRef<FJsonObject> Dispatch(
		UCLLobbySubsystem* Lobby,
		const TSharedPtr<FJsonObject>& Root,
		FGuid* FallbackSeat = nullptr);
};
