#include "Player/CLIntentReceiver.h"
#include "Player/CLPlayerActionRouter.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLWeaponMotorComponent.h"

UCLIntentReceiver::UCLIntentReceiver()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector2D UCLIntentReceiver::TakeLookDelta()
{
	const FVector2D Out = AgentLook;
	AgentLook = FVector2D::ZeroVector;
	return Out;
}

void UCLIntentReceiver::ClearLookDelta()
{
	AgentLook = FVector2D::ZeroVector;
}

void UCLIntentReceiver::ApplyAgentIntent(const FCLAgentIntent& Intent)
{
	AgentMove = Intent.Move;
	AgentLook += Intent.Look;
	bAgentSprint = Intent.bSprint;
	bAgentCrouch = Intent.bCrouch;
	bAgentADS = Intent.bADS;
	bAgentFire = Intent.bFire;
	if (Intent.bJump) { bAgentJump = true; }
	if (Intent.bDodge) { bAgentDodge = true; }
	if (Intent.bDash) { bAgentDash = true; }
	if (Intent.bReload) { bAgentReload = true; }
	if (Intent.bSwap) { bAgentSwap = true; }
	if (Intent.bSlide) { bAgentSlide = true; }
	if (Intent.bAirDive) { bAgentAirDive = true; }
	if (Intent.bMelee) { bAgentMelee = true; }
	if (Intent.bWeaponPrimary) { bAgentWeaponPrimary = true; }
	if (Intent.bWeaponSpecial) { bAgentWeaponSpecial = true; }
	if (!Intent.SightId.IsNone()) { AgentSightId = Intent.SightId; }
}

void UCLIntentReceiver::ApplyAgentIntent(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire,
	bool bJump, bool bDodge, bool bDash, bool bReload, bool bSwap)
{
	FCLAgentIntent Intent;
	Intent.Move = MoveXY;
	Intent.Look = LookDelta;
	Intent.bSprint = bSprint;
	Intent.bCrouch = bCrouch;
	Intent.bADS = bADS;
	Intent.bFire = bFire;
	Intent.bJump = bJump;
	Intent.bDodge = bDodge;
	Intent.bDash = bDash;
	Intent.bReload = bReload;
	Intent.bSwap = bSwap;
	ApplyAgentIntent(Intent);
}

void UCLIntentReceiver::LatchWhileHolds(bool bADS, bool bFire)
{
	if (bADS)
	{
		bAgentADS = true;
	}
	if (bFire)
	{
		bAgentFire = true;
	}
}

void UCLIntentReceiver::ClearAgentIntent()
{
	AgentMove = FVector2D::ZeroVector;
	AgentLook = FVector2D::ZeroVector;
	bAgentSprint = false;
	bAgentCrouch = false;
	bAgentADS = false;
	bAgentFire = false;
	bAgentJump = false;
	bAgentDodge = false;
	bAgentDash = false;
	bAgentReload = false;
	bAgentSwap = false;
	bAgentSlide = false;
	bAgentAirDive = false;
	bAgentMelee = false;
	bAgentWeaponPrimary = false;
	bAgentWeaponSpecial = false;
	AgentSightId = NAME_None;
}

void UCLIntentReceiver::ConsumeAgentPulses()
{
	ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner());
	if (!Char || !Char->IsCombatAlive())
	{
		bAgentJump = false;
		bAgentDodge = false;
		bAgentDash = false;
		bAgentReload = false;
		bAgentSwap = false;
		bAgentSlide = false;
		bAgentAirDive = false;
		bAgentMelee = false;
		bAgentWeaponPrimary = false;
		bAgentWeaponSpecial = false;
		return;
	}
	if (bAgentJump)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Jump);
		bAgentJump = false;
	}
	if (bAgentDodge)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Dodge);
		bAgentDodge = false;
	}
	if (bAgentDash)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Dash);
		bAgentDash = false;
	}
	if (bAgentReload)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Reload);
		bAgentReload = false;
	}
	if (bAgentSwap)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Swap);
		bAgentSwap = false;
	}
	if (bAgentSlide)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Slide);
		bAgentSlide = false;
	}
	if (bAgentAirDive)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::AirDive);
		bAgentAirDive = false;
	}
	if (bAgentMelee)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::Melee);
		bAgentMelee = false;
	}
	if (bAgentWeaponPrimary)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::WeaponPrimary);
		bAgentWeaponPrimary = false;
	}
	if (bAgentWeaponSpecial)
	{
		CLPlayerActionRouter::DispatchPulse(Char, ECLBindableAction::WeaponSpecial);
		bAgentWeaponSpecial = false;
	}
	if (!AgentSightId.IsNone() && Char->GetWeaponMotor())
	{
		Char->GetWeaponMotor()->SetSight(AgentSightId);
		AgentSightId = NAME_None;
	}
}
