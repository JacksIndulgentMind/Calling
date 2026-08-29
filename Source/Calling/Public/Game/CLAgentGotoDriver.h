#pragma once

#include "CoreMinimal.h"
#include "Nav/CLNavAbilityExec.h"

class ACLPlayerCharacter;
class UWorld;

struct CALLING_API FCLAgentGotoDriver
{
	bool bActive = false;
	bool bPartial = false;
	bool bFlight = false;
	/** Path came from Recast FindPath (incl. partial). Trust it over the void floor probe. */
	bool bNavPath = false;
	TArray<FVector> Path;
	TArray<uint8> PathAirDive;
	int32 Index = 0;
	FVector Goal = FVector::ZeroVector;
	int32 RepathLeft = 0;
	float JumpCooldown = 0.f;
	float StuckSeconds = 0.f;
	float LastWpDist = -1.f;
	FCLNavAbilityExec Flight;

	/** Live diag for /state + LogCallingGoto (updated each Tick). */
	FName SteerReason = NAME_None;
	FVector SteerAt = FVector::ZeroVector;
	float DistLip = -1.f;
	float DistWp = -1.f;
	float DistXYToWp = -1.f;
	float DeltaZToWp = 0.f;
	bool bLaunchOk = false;
	bool bMoveBlocked = false;
	FVector2D LastMoveXY = FVector2D::ZeroVector;
	FName FwdKind = NAME_None;
	float FwdDist = -1.f;

	void Cancel();
	bool Start(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError, bool bFromRepath = false);
	void Tick(float DeltaSeconds, UWorld* World, ACLPlayerCharacter* Char);
};
