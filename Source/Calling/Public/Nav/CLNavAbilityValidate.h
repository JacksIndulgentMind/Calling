#pragma once

#include "CoreMinimal.h"

class ARecastNavMesh;
struct FCLNavTune;
struct FCLMovementTune;

struct FCLNavAbilityValidateResult
{
	bool bLocksOk = true;
	bool bApplyAirDiveLink = true;
	FString Message;
};

namespace CLNavAbilityValidate
{
	/** Compare movement tune vs NavTune. Does not rewrite NavTune. */
	FCLNavAbilityValidateResult Check(const FCLMovementTune& Move, const FCLNavTune& Nav, float SurvivingDropCm);
	void Report(UObject* WorldContext, const FCLNavAbilityValidateResult& Result);
}
