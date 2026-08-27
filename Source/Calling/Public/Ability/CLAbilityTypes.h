#pragma once

#include "CoreMinimal.h"
#include "CLAbilityTypes.generated.h"

	UENUM(BlueprintType)
	enum class ECLAbilitySlot : uint8
	{
		Grenade UMETA(DisplayName = "Grenade"),
		Shield UMETA(DisplayName = "Shield"),
		Evasion UMETA(DisplayName = "Evasion"),
		Dash UMETA(DisplayName = "Dash"),
		Melee UMETA(DisplayName = "Melee"),
		Jump UMETA(DisplayName = "Jump"),
		Super UMETA(DisplayName = "Super")
	};

UENUM(BlueprintType)
enum class ECLJumpStyle : uint8
{
	RocketPulse UMETA(DisplayName = "Rocket Pulse"),
	InertiaDamp UMETA(DisplayName = "Inertia Dampers")
};

inline bool CLParseAbilitySlot(const FString& Name, ECLAbilitySlot& OutSlot)
{
	if (Name.Equals(TEXT("grenade"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Grenade; return true; }
	if (Name.Equals(TEXT("shield"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Shield; return true; }
	if (Name.Equals(TEXT("evasion"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Evasion; return true; }
	if (Name.Equals(TEXT("dash"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Dash; return true; }
	if (Name.Equals(TEXT("melee"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Melee; return true; }
	if (Name.Equals(TEXT("jump"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Jump; return true; }
	if (Name.Equals(TEXT("super"), ESearchCase::IgnoreCase)) { OutSlot = ECLAbilitySlot::Super; return true; }
	return false;
}
