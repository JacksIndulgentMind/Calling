#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Same-PC listen host beacon + debug log. Not a net channel. */
namespace CLLoopbackJoin
{
	bool ShowUi();
	FString DefaultConnect();
	FString BeaconPath();
	FString LogPath();
	void WriteBeacon(UWorld* World);
	void ClearBeacon();
	FString ReadBeaconConnect();
	FString ResolveConnect(const FString& Selected);
	void AppendLog(const FString& Line);
	int32 ListenPort(UWorld* World);
}
