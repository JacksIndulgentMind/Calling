#include "Nav/CLNavArea_AirDive.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CLNavArea_AirDive)

UCLNavArea_AirDive::UCLNavArea_AirDive()
{
	// FindPath otherwise treats AirDive links like walk (cost 1) and prefers short
	// AirDive chords off spawn/terrace over the DropDown+ramp corridor. LongJump is 25.
	DefaultCost = 50.f;
	DrawColor = FColor(80, 180, 255);
}
