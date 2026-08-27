#pragma once

#include "CoreMinimal.h"

class ARecastNavMesh;

namespace DLNavLinkPolicy
{
	/** Recast agent size + generated jump links from NavTune.json. SurvivingDropCm is map-local. */
	void ApplyToRecast(ARecastNavMesh& Recast, float SurvivingDropCm);
}
