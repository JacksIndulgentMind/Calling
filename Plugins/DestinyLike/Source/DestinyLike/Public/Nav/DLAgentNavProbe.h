#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UWorld;
class AActor;
class ACharacter;

enum class EDLFwdKind : uint8
{
	Open,
	Walk,
	Drop,
	Cover,
	JumpUp,
	JumpDown,
	Wall
};

struct FDLAgentBlockHit
{
	float Dist = 1800.f;
	EDLFwdKind Kind = EDLFwdKind::Open;
};

namespace DLAgentNavProbe
{
	const TCHAR* FwdKindName(EDLFwdKind Kind);
	int32 NavTileCount(UWorld* World);
	FDLAgentBlockHit ProbeBlock(UWorld* World, const AActor* Ignore, const FVector& Start, const FVector& Dir, float FeetZ);
	float FloorDropCm(UWorld* World, const AActor* Ignore, const FVector& Loc, float HalfHeight, const FVector& Dir, float AheadCm);
	void FillStateJson(const TSharedRef<FJsonObject>& Root, UWorld* World, const ACharacter* Char);
}
