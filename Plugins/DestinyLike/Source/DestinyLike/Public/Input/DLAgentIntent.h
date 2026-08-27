#pragma once

#include "CoreMinimal.h"

enum class EDLLookMode : uint8
{
	None,
	Delta,
	AbsYaw,
	AbsPitch,
	AbsBoth
};

/** Look is a command variant: delta pulse, absolute yaw/pitch, or none. */
struct DESTINYLIKE_API FDLLookCommand
{
	EDLLookMode Mode = EDLLookMode::None;
	FVector2D Value = FVector2D::ZeroVector;

	FVector2D GetDelta() const
	{
		return Mode == EDLLookMode::Delta ? Value : FVector2D::ZeroVector;
	}

	bool HasAbsolute() const
	{
		return Mode == EDLLookMode::AbsYaw || Mode == EDLLookMode::AbsPitch || Mode == EDLLookMode::AbsBoth;
	}

	static FDLLookCommand MakeDelta(float Yaw, float Pitch)
	{
		FDLLookCommand Cmd;
		if (FMath::IsNearlyZero(Yaw) && FMath::IsNearlyZero(Pitch))
		{
			return Cmd;
		}
		Cmd.Mode = EDLLookMode::Delta;
		Cmd.Value = FVector2D(Yaw, Pitch);
		return Cmd;
	}

	static FDLLookCommand MakeAbsolute(bool bYaw, float Yaw, bool bPitch, float Pitch)
	{
		FDLLookCommand Cmd;
		if (bYaw && bPitch)
		{
			Cmd.Mode = EDLLookMode::AbsBoth;
			Cmd.Value = FVector2D(Yaw, Pitch);
		}
		else if (bYaw)
		{
			Cmd.Mode = EDLLookMode::AbsYaw;
			Cmd.Value = FVector2D(Yaw, 0.f);
		}
		else if (bPitch)
		{
			Cmd.Mode = EDLLookMode::AbsPitch;
			Cmd.Value = FVector2D(0.f, Pitch);
		}
		return Cmd;
	}
};

/**
 * One tick of pawn intent. Cursor HTTP, Recast /goto, and in-game playbooks
 * all apply this through ADLPlayerCharacter::ApplyAgentIntent. Holdables latch;
 * look and pulse flags are consumed.
 */
struct DESTINYLIKE_API FDLAgentIntent
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
