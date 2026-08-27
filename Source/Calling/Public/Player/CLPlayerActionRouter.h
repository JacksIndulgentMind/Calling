#pragma once

#include "CoreMinimal.h"
#include "Input/CLInputTypes.h"

class ACLPlayerCharacter;

namespace CLPlayerActionRouter
{
	CALLING_API void DispatchPulse(ACLPlayerCharacter* Char, ECLBindableAction Action);
}
