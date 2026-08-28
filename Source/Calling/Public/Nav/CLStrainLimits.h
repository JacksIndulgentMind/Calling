#pragma once

#include "CoreMinimal.h"

struct FCLStrainLimits
{
	float MaxFallBeforeCriticalCm = 3000.f;
};

namespace CLStrainLimits
{
	const FCLStrainLimits& Get();
}
