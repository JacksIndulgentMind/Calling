#include "Player/CLPlayerActionRouter.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLWeaponMotorComponent.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Core/CLTypes.h"

void CLPlayerActionRouter::DispatchPulse(ACLPlayerCharacter* Char, ECLBindableAction Action)
{
	if (!Char || !Char->IsCombatAlive())
	{
		return;
	}
	switch (Action)
	{
	case ECLBindableAction::Jump:
		Char->Jump();
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryMantle();
		}
		break;
	case ECLBindableAction::Slide:
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->RequestSlide();
		}
		break;
	case ECLBindableAction::AirDive:
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryAirDive();
		}
		break;
	case ECLBindableAction::Dodge:
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDodge();
		}
		break;
	case ECLBindableAction::Reload:
		if (UCLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			Motor->StartReload();
		}
		break;
	case ECLBindableAction::Swap:
		if (UCLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			const ECLWeaponSlot Next = Motor->GetActiveItem().Weapon.Slot == ECLWeaponSlot::Primary
				? ECLWeaponSlot::Special : ECLWeaponSlot::Primary;
			Motor->SwapToSlot(Next);
		}
		break;
	case ECLBindableAction::WeaponPrimary:
		if (UCLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			Motor->SwapToSlot(ECLWeaponSlot::Primary);
		}
		break;
	case ECLBindableAction::WeaponSpecial:
		if (UCLWeaponMotorComponent* Motor = Char->GetWeaponMotor())
		{
			Motor->SwapToSlot(ECLWeaponSlot::Special);
		}
		break;
	case ECLBindableAction::Grenade:
		if (UCLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryGrenade(); }
		break;
	case ECLBindableAction::Melee:
		if (UCLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryMelee(); }
		break;
	case ECLBindableAction::Dash:
		if (UCLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryDash(); }
		break;
	case ECLBindableAction::Shield:
		if (UCLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryShield(); }
		break;
	case ECLBindableAction::Evasion:
		if (UCLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TryEvasion(); }
		break;
	case ECLBindableAction::Super:
		if (UCLAbilityLoadoutComponent* Abs = Char->GetAbilities()) { Abs->TrySuper(); }
		break;
	default:
		break;
	}
}
