#pragma once

#include "CoreMinimal.h"

enum class ECLLookMode : uint8
{
	None,
	Delta,
	AbsYaw,
	AbsPitch,
	AbsBoth
};

/** Look is a command variant: delta pulse, absolute yaw/pitch, or none. */
struct CALLING_API FCLLookCommand
{
	ECLLookMode Mode = ECLLookMode::None;
	FVector2D Value = FVector2D::ZeroVector;

	FVector2D GetDelta() const
	{
		return Mode == ECLLookMode::Delta ? Value : FVector2D::ZeroVector;
	}

	bool HasAbsolute() const
	{
		return Mode == ECLLookMode::AbsYaw || Mode == ECLLookMode::AbsPitch || Mode == ECLLookMode::AbsBoth;
	}

	static FCLLookCommand MakeDelta(float Yaw, float Pitch)
	{
		FCLLookCommand Cmd;
		if (FMath::IsNearlyZero(Yaw) && FMath::IsNearlyZero(Pitch))
		{
			return Cmd;
		}
		Cmd.Mode = ECLLookMode::Delta;
		Cmd.Value = FVector2D(Yaw, Pitch);
		return Cmd;
	}

	static FCLLookCommand MakeAbsolute(bool bYaw, float Yaw, bool bPitch, float Pitch)
	{
		FCLLookCommand Cmd;
		if (bYaw && bPitch)
		{
			Cmd.Mode = ECLLookMode::AbsBoth;
			Cmd.Value = FVector2D(Yaw, Pitch);
		}
		else if (bYaw)
		{
			Cmd.Mode = ECLLookMode::AbsYaw;
			Cmd.Value = FVector2D(Yaw, 0.f);
		}
		else if (bPitch)
		{
			Cmd.Mode = ECLLookMode::AbsPitch;
			Cmd.Value = FVector2D(0.f, Pitch);
		}
		return Cmd;
	}
};

/**
 * One tick of pawn intent. Cursor HTTP, Recast /goto, and BotBooks
 * all apply this through ACLPlayerCharacter::ApplyAgentIntent. Holdables latch;
 * look and pulse flags are consumed.
 */
struct CALLING_API FCLAgentIntent
{
	FVector2D Move = FVector2D::ZeroVector;
	FVector2D Look = FVector2D::ZeroVector;
	bool bSprint = false;
	bool bCrouch = false;
	bool bADS = false;
	bool bFire = false;
	bool bJump = false;
	bool bDodge = false;
	bool bDash = false;
	bool bReload = false;
	bool bSwap = false;
	bool bSlide = false;
	bool bAirDive = false;
	bool bMelee = false;
	bool bWeaponPrimary = false;
	bool bWeaponSpecial = false;
	FName SightId = NAME_None;
};
