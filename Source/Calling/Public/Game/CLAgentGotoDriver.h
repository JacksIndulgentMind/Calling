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
	TArray<FVector> Path;
	TArray<uint8> PathAirDive;
	int32 Index = 0;
	FVector Goal = FVector::ZeroVector;
	int32 RepathLeft = 0;
	float JumpCooldown = 0.f;
	float StuckSeconds = 0.f;
	float LastWpDist = -1.f;
	FCLNavAbilityExec Flight;

	void Cancel();
	bool Start(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError, bool bFromRepath);
	void Tick(float DeltaSeconds, UWorld* World, ACLPlayerCharacter* Char);
};
