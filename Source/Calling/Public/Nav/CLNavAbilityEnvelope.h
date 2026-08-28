#pragma once

#include "CoreMinimal.h"
#include "Core/CLTunes.h"
#include "Nav/CLStrainLimits.h"

struct FCLNavTune;

/** Algorithmic range boxes for BotBook *-to leaves. Derived from movement tune, not map centimeters. */
struct CALLING_API FCLNavAbilityBox
{
	float MinDistXY = 0.f;
	float MaxDistXY = 0.f;
	float MinDeltaZ = -1.0e7f;
	float MaxDeltaZ = 1.0e7f;
	float ReleaseDistXY = 140.f;
	float CoastXY = 40.f;
};

enum class ECLLaunchRing : uint8
{
	Inner,
	Mid,
	Outer
};

/** One toroidal cell: height slice × DistXY ring → full Launch recipe. */
struct CALLING_API FCLLaunchRecipe
{
	bool bValid = false;
	int32 Jumps = 0;
	int32 SliceJumps = 0;
	ECLLaunchRing Ring = ECLLaunchRing::Inner;
	float SliceZCm = 0.f;
	float RingMinXY = 0.f;
	float RingMaxXY = 0.f;
	bool bJumpStill = false;
	bool bPinUntilLand = false;
	bool bShortPin = false;
	float AirSteerMul = 0.f;
	const TCHAR* SliceName = TEXT("none");
	const TCHAR* RingName = TEXT("none");
};

namespace CLNavAbility
{
	float SprintCmPerSec(const FCLMovementTune& Tune);
	float CoastAfterReleaseCm(float SpeedXY, float BrakeAccel);
	float JumpApexUpCm(const FCLMovementTune& Tune, int32 JumpsUsed);
	float ReleaseDistXY(const FCLNavAbilityBox& Box, float LandingRadius);

	/** Horizontal reach while slamming from DropCm below the start (hang then 8G). Landing feel only. */
	float PhysicsAirDiveXY(const FCLMovementTune& Tune, float DropCm);
	/** Stick forward through stacked jumps, AirControl 0.35. Modest XY, not dive speed. */
	float JumpSteerXY(const FCLMovementTune& Tune, int32 JumpsUsed);
	/** Hang at AirDiveMaxXY, then remaining drop with stick pinned at 1g (not 8G slam). */
	float PinnedDiveXY(const FCLMovementTune& Tune, float DropCm);
	/** Recast search + Launch envelope: JumpSteerXY(3) + PinnedDiveXY(drop). */
	float MaxLaunchXY(const FCLMovementTune& Tune, float DropCm);
	/** Stick-forward triple that returns to lip height (full up+down of JumpApex). No PinnedDiveXY. Recast JumpLength. */
	float SamePlaneJumpLengthCm(const FCLMovementTune& Tune);
	/** Epic parabola intercept with the launch plane: x0 = L·(2H+2√(H(H+D)))/(D+2H+2√(H(H+D))). */
	float RecastJumpLaunchPlaneInterceptCm(float JumpLength, float JumpHeight, float JumpMaxDepth);
	/** Drop needed so hang+pinned 1g XY reach covers DistXY. */
	float MinDropCmForDistXY(const FCLMovementTune& Tune, float DistXY);
	/** Jump pulses to get above the pad with enough drop for DistXY. */
	int32 JumpsToLaunch(const FCLMovementTune& Tune, const FVector& From, const FVector& To);
	/** Hang-phase XY while slamming (AirDiveMaxXY * DiveHangSeconds). Still vs forward uses this. */
	float HangReachXY(const FCLMovementTune& Tune);
	/** Recast AirDive chord drop: strain max fall, not surviving-rescue. */
	inline float AirDiveRefDropCm()
	{
		return CLStrainLimits::Get().MaxFallBeforeCriticalCm;
	}
	/** Recast end-poly slack. Not added to JumpMaxDepth; the island sits this much shallower than strain. */
	inline constexpr float AirDiveChordEndZTolCm = 300.f;
	/** Survivable fall from jump apex to the island (strain minus end-tol). */
	inline float AirDivePadDropFromApexCm()
	{
		return FMath::Max(400.f, AirDiveRefDropCm() - AirDiveChordEndZTolCm);
	}
	/** Lip-floor to island: apex-drop minus full triple. Movement tune only — not Recast jumpApexCm. */
	inline float AirDivePadDropFromLipCm(const FCLMovementTune& Tune)
	{
		const float Triple = JumpApexUpCm(Tune, FMath::Max(1, Tune.MaxJumps));
		return FMath::Max(800.f, AirDivePadDropFromApexCm() - Triple);
	}
	/** Recast long-recipe JumpMaxDepth: Abs(triple apex − maxFall). Never raw 3000/30000. */
	inline float AirDiveChordMaxDepthCm(const FCLMovementTune& Tune)
	{
		const float Apex = JumpApexUpCm(Tune, FMath::Max(1, Tune.MaxJumps));
		return FMath::Abs(Apex - AirDiveRefDropCm());
	}
	/** CharacterMovement MaxAcceleration default; JumpSteerXY uses this * AirControl. */
	inline constexpr float JumpAirAccelCm = 2048.f;
	/** Airborne, above the pad, and the launch box still covers To. */
	bool ReadyToAirDive(const FCLMovementTune& Tune, float MaxAccel, float SpeedXY, const FVector& From, const FVector& To);
	/** Bake + runtime search: min(MaxLaunchXY, NavTune cap). Cap 0 = physics only. */
	float SearchRadiusCm(const FCLMovementTune& Tune, const FCLNavTune& NavTune, float DropCm);
	/** Greybox island chord = Recast launch-plane intercept (x0). Rim inset is applied at Ends. Recast knobs must not move this after freeze. */
	float AirDivePadPlaceChordCm(const FCLMovementTune& Tune);
	/** Locked greybox rim inset. Recast JumpDistanceFromEdge must not move the pad. */
	inline float AirDivePadRimInsetCm(float JumpDistanceFromEdgeCm)
	{
		(void)JumpDistanceFromEdgeCm;
		return 200.f;
	}
	/** Hop-bounds expand (not a Recast JumpLength ceiling). Lip-to-pad DistXY plus edge. */
	inline float AirDiveBakeJumpLengthCm(const FCLMovementTune& Tune, float JumpDistanceFromEdgeCm)
	{
		const float Place = AirDivePadPlaceChordCm(Tune);
		const float Inward = AirDivePadRimInsetCm(JumpDistanceFromEdgeCm);
		return FMath::Max(Place - Inward + JumpDistanceFromEdgeCm, 800.f);
	}

	FCLNavAbilityBox JumpTo(const FCLMovementTune& Tune, int32 JumpsUsed);
	bool JumpToInEnvelope(const FCLMovementTune& Tune, const FVector& From, const FVector& To);
	FCLNavAbilityBox AirDiveTo(const FCLMovementTune& Tune, float MaxAccel, float SpeedXY, float DeltaZ);
	FCLNavAbilityBox SlideTo(const FCLMovementTune& Tune);
	FCLNavAbilityBox DashTo(const FCLMovementTune& Tune);
	FCLNavAbilityBox DodgeTo(const FCLMovementTune& Tune);

	bool InEnvelope(const FCLNavAbilityBox& Box, const FVector& From, const FVector& To);
	bool ShouldReleaseStick(const FCLNavAbilityBox& Box, float DistXY);
	bool ShouldAbandon(const FCLNavAbilityBox& Box, const FVector& From, const FVector& To, bool bOnGround, bool bMechanicSeen);
	/** Capsule center above a floor-top marker (not the pit under a lintel). */
	bool StandingOnGoalFloor(const FVector& Loc, const FVector& GoalFloor);

	/** Jump-to fails, slide/strafe fail, air-dive launch can still reach. */
	bool LaunchInEnvelope(const FCLMovementTune& Tune, const FCLNavTune& NavTune, const FVector& From, const FVector& To);
	bool IsAirDivePathSegment(const FCLMovementTune& Tune, const FCLNavTune& NavTune, const FVector& From, const FVector& To);
	bool IsAirDiveArea(const UClass* AreaClass);
	/** Height slice × DistXY ring for this landing. Invalid if outside the hull or an empty cell. */
	bool LookupLaunchRecipe(const FCLMovementTune& Tune, const FCLNavTune& NavTune, const FVector& From,
		const FVector& To, FCLLaunchRecipe& Out);
}
