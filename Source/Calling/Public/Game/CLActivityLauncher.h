#pragma once

#include "CoreMinimal.h"
#include "Core/CLTypes.h"

class UObject;

namespace CLActivityLauncher
{
	CALLING_API void Travel(UObject* WorldContext, ECLSceneId Scene, int32 RaidChamberIndex = 0);
	CALLING_API void ExitToSocial(UObject* WorldContext);
}
