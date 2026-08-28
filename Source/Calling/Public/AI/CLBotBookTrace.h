#pragma once

#include "CoreMinimal.h"

struct FCLNavAbilityBox;

namespace CLBotBookTrace
{
	bool IsOn();

	void LeafStart(const TCHAR* Verb, const TCHAR* NodeId, FName Marker, const FVector& From, const FVector& Goal);
	void LeafSettle(const TCHAR* Verb, const TCHAR* NodeId, const TCHAR* Outcome, float Elapsed,
		const FVector& Loc, const FVector& Goal);

	void ExecStart(const TCHAR* Mode, const FVector& From, const FVector& Goal, const FCLNavAbilityBox& Box,
		const TCHAR* Sub, int32 Jumps);
	void Phase(const TCHAR* Mode, const TCHAR* What, float Elapsed, const FVector& Loc, const FVector& Goal,
		const FVector& Vel, const TCHAR* Extra = TEXT(""));
	void Sample(const TCHAR* Mode, const TCHAR* Phase, float Elapsed, float Dt, const FVector& Loc,
		const FVector& Vel, const FVector& Goal);
	void VelInterval(const TCHAR* Mode, const TCHAR* Phase, float MinXY, float MaxXY, float MeanXY,
		float MinVz, float MaxVz);
	void Miss(const TCHAR* Mode, const TCHAR* Result, const TCHAR* Phase, const FVector& Loc, const FVector& Goal,
		const FCLNavAbilityBox& Box, float ReleaseDist, bool bOnPad);
	void GotoArm(const TCHAR* Arm, const FVector& From, const FVector& Dest, bool bPartial, int32 PathPts);
}
