#pragma once

#include "CoreMinimal.h"
#include "Input/DLInputTypes.h"

class ADLPlayerCharacter;

namespace DLPlayerActionRouter
{
	DESTINYLIKE_API void DispatchPulse(ADLPlayerCharacter* Char, EDLBindableAction Action);
}
