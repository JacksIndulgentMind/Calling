#pragma once

#include "CoreMinimal.h"
#include "Core/DLTypes.h"

class UObject;

namespace DLActivityLauncher
{
	DESTINYLIKE_API void Travel(UObject* WorldContext, EDLSceneId Scene, int32 RaidChamberIndex = 0);
	DESTINYLIKE_API void ExitToSocial(UObject* WorldContext);
}
