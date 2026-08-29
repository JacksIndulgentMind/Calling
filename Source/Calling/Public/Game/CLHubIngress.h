#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Connect-mode handshake for hub HTTP/WS.
 * `local` (default) executes on this process. `proxy` on the host forwards
 * into the guest's same ingress as HTTP 18767 — guest Dispatch cannot tell
 * which path was used.
 */
struct FCLHubConnect
{
	bool bProxy = false;
	FGuid TargetInstance;
	FGuid ViaSeat;
};

namespace CLHubIngress
{
	FCLHubConnect Parse(
		const TSharedPtr<FJsonObject>& Root,
		const FString& HeaderMode,
		const FString& HeaderTarget,
		const FString& QueryMode = FString(),
		const FString& QueryTarget = FString());

	void StripProxyFields(const TSharedPtr<FJsonObject>& Root);
}
