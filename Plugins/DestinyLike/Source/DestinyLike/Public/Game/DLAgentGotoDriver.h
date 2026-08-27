#pragma once

#include "CoreMinimal.h"

class ADLPlayerCharacter;
class UWorld;

struct DESTINYLIKE_API FDLAgentGotoDriver
{
	bool bActive = false;
	bool bPartial = false;
	TArray<FVector> Path;
	int32 Index = 0;
	FVector Goal = FVector::ZeroVector;
	int32 RepathLeft = 0;
	float JumpCooldown = 0.f;
	float StuckSeconds = 0.f;
	float LastWpDist = -1.f;

	void Cancel();
	bool Start(UWorld* World, ADLPlayerCharacter* Char, const FVector& Dest, FString& OutError, bool bFromRepath);
	void Tick(float DeltaSeconds, UWorld* World, ADLPlayerCharacter* Char);
};
