#include "Player/DLPlayerActionRouter.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLWeaponMotorComponent.h"
#include "Player/DLAbilityLoadoutComponent.h"
#include "Core/DLTypes.h"

void DLPlayerActionRouter::DispatchPulse(ADLPlayerCharacter* Char, EDLBindableAction Action)
{
	if (!Char || !Char->IsCombatAlive())
	{
		return;
	}
	switch (Action)
	{
	case EDLBindableAction::Jump:
		Char->Jump();
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryMantle();
		}
		break;
	case EDLBindableAction::Slide:
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->RequestSlide();
		}
		break;
	case EDLBindableAction::AirDive:
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryAirDive();
		}
		break;
	case EDLBindableAction::Dodge:
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDodge();
		}
		break;
	case EDLBindableAction::Reload:
		if (UDLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			Motor->StartReload();
		}
		break;
	case EDLBindableAction::Swap:
		if (UDLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			const EDLWeaponSlot Next = Motor->GetActiveItem().Weapon.Slot == EDLWeaponSlot::Primary
				? EDLWeaponSlot::Special : EDLWeaponSlot::Primary;
			Motor->SwapToSlot(Next);
		}
		break;
	case EDLBindableAction::WeaponPrimary:
		if (UDLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			Motor->SwapToSlot(EDLWeaponSlot::Primary);
		}
		break;
	case EDLBindableAction::WeaponSpecial:
		if (UDLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			Motor->SwapToSlot(EDLWeaponSlot::Special);
		}
		break;
	case EDLBindableAction::Grenade:
		if (UDLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryGrenade(); }
		break;
	case EDLBindableAction::Melee:
		if (UDLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryMelee(); }
		break;
	case EDLBindableAction::Dash:
		if (UDLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryDash(); }
		break;
	case EDLBindableAction::Shield:
		if (UDLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryShield(); }
		break;
	case EDLBindableAction::Evasion:
		if (UDLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryEvasion(); }
		break;
	case EDLBindableAction::Super:
		if (UDLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TrySuper(); }
		break;
	default:
		break;
	}
}
