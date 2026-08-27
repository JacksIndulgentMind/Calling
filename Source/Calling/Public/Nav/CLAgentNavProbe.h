#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UWorld;
class AActor;
class ACharacter;

enum class ECLFwdKind : uint8
{
	Open,
	Walk,
	Drop,
	Cover,
	JumpUp,
	JumpDown,
	Wall
};

struct FCLAgentBlockHit
{
	float Dist = 1800.f;
	ECLFwdKind Kind = ECLFwdKind::Open;
};

namespace CLAgentNavProbe
{
	const TCHAR* FwdKindName(ECLFwdKind Kind);
	int32 NavTileCount(UWorld* World);
	FCLAgentBlockHit ProbeBlock(UWorld* World, const AActor* Ignore, const FVector& Start, const FVector& Dir, float FeetZ);
	float FloorDropCm(UWorld* World, const AActor* Ignore, const FVector& Loc, float HalfHeight, const FVector& Dir, float AheadCm);
	void FillStateJson(const TSharedRef<FJsonObject>& Root, UWorld* World, const ACharacter* Char);
}
