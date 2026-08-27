#pragma once

#include "CoreMinimal.h"
#include "DLAbilityTypes.generated.h"

	UENUM(BlueprintType)
	enum class EDLAbilitySlot : uint8
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
enum class EDLJumpStyle : uint8
{
	RocketPulse UMETA(DisplayName = "Rocket Pulse"),
	InertiaDamp UMETA(DisplayName = "Inertia Dampers")
};

inline bool DLParseAbilitySlot(const FString& Name, EDLAbilitySlot& OutSlot)
{
	if (Name.Equals(TEXT("grenade"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Grenade; return true; }
	if (Name.Equals(TEXT("shield"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Shield; return true; }
	if (Name.Equals(TEXT("evasion"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Evasion; return true; }
	if (Name.Equals(TEXT("dash"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Dash; return true; }
	if (Name.Equals(TEXT("melee"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Melee; return true; }
	if (Name.Equals(TEXT("jump"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Jump; return true; }
	if (Name.Equals(TEXT("super"), ESearchCase::IgnoreCase)) { OutSlot = EDLAbilitySlot::Super; return true; }
	return false;
}
