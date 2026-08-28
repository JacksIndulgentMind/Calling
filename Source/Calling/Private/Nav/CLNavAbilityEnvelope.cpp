#include "Nav/CLNavAbilityEnvelope.h"
#include "Nav/CLNavTune.h"
#include "Nav/CLNavArea_AirDive.h"

float CLNavAbility::SprintCmPerSec(const FCLMovementTune& Tune)
{
	return Tune.BaseWalkSpeed * Tune.SprintSpeedMultiplier;
}

float CLNavAbility::CoastAfterReleaseCm(float SpeedXY, float BrakeAccel)
{
	const float V = FMath::Max(0.f, SpeedXY);
	const float A = FMath::Max(1.f, BrakeAccel);
	return (V * V) / (2.f * A);
}

float CLNavAbility::JumpApexUpCm(const FCLMovementTune& Tune, int32 JumpsUsed)
{
	const float G = FMath::Max(1.f, Tune.GravityZ);
	if (JumpsUsed <= 0)
	{
		return 0.f;
	}
	// RocketPulse: each air jump does Max(Vz,0)+DoubleJumpZ. Envelope peak = mash
	// all pulses at takeoff (upper bound the leaf / Recast JumpHeight must cover).
	const float V0 = FMath::Max(0.f, Tune.JumpZVelocity);
	const float VPulse = Tune.DoubleJumpZVelocity > 0.f ? Tune.DoubleJumpZVelocity : V0 * 0.4f;
	const int32 AirPulses = FMath::Clamp(JumpsUsed - 1, 0, FMath::Max(0, Tune.MaxJumps - 1));
	const float VPeak = V0 + static_cast<float>(AirPulses) * VPulse;
	return (VPeak * VPeak) / (2.f * G);
}

float CLNavAbility::PhysicsAirDiveXY(const FCLMovementTune& Tune, float DropCm)
{
	const float G = FMath::Max(1.f, Tune.GravityZ);
	const float HangT = FMath::Max(0.f, Tune.DiveHangSeconds);
	const float HangG = FMath::Max(0.01f, Tune.DiveHangGravity) * G;
	const float HangFall = 0.5f * HangG * HangT * HangT;
	const float Remain = FMath::Max(0.f, DropCm - HangFall);
	const float SlamG = FMath::Max(0.01f, Tune.DiveFallGravity) * G;
	const float SlamT = Remain > 1.f ? FMath::Sqrt((2.f * Remain) / SlamG) : 0.f;
	return FMath::Max(0.f, Tune.AirDiveMaxXY) * (HangT + SlamT);
}

float CLNavAbility::JumpSteerXY(const FCLMovementTune& Tune, int32 JumpsUsed)
{
	const float Apex = JumpApexUpCm(Tune, JumpsUsed);
	const float G = FMath::Max(1.f, Tune.GravityZ);
	const float T = Apex > 1.f ? FMath::Sqrt((2.f * Apex) / G) : 0.f;
	const float Accel = JumpAirAccelCm * FMath::Clamp(Tune.AirControl, 0.f, 1.f);
	const float FromRest = 0.5f * Accel * T * T;
	const float WalkCap = Tune.BaseWalkSpeed * T;
	return FMath::Min(FromRest, WalkCap);
}

float CLNavAbility::PinnedDiveXY(const FCLMovementTune& Tune, float DropCm)
{
	const float G = FMath::Max(1.f, Tune.GravityZ);
	const float HangT = FMath::Max(0.f, Tune.DiveHangSeconds);
	const float HangG = FMath::Max(0.01f, Tune.DiveHangGravity) * G;
	const float HangFall = 0.5f * HangG * HangT * HangT;
	const float Remain = FMath::Max(0.f, DropCm - HangFall);
	const float FallT = Remain > 1.f ? FMath::Sqrt((2.f * Remain) / G) : 0.f;
	return FMath::Max(0.f, Tune.AirDiveMaxXY) * (HangT + FallT);
}

float CLNavAbility::MaxLaunchXY(const FCLMovementTune& Tune, float DropCm)
{
	return JumpSteerXY(Tune, FMath::Max(1, Tune.MaxJumps)) + PinnedDiveXY(Tune, DropCm);
}

float CLNavAbility::SamePlaneJumpLengthCm(const FCLMovementTune& Tune)
{
	const int32 J = FMath::Max(1, Tune.MaxJumps);
	const float Apex = JumpApexUpCm(Tune, J);
	const float G = FMath::Max(1.f, Tune.GravityZ);
	const float TUp = Apex > 1.f ? FMath::Sqrt((2.f * Apex) / G) : 0.f;
	const float T = 2.f * TUp;
	const float Accel = JumpAirAccelCm * FMath::Clamp(Tune.AirControl, 0.f, 1.f);
	const float FromRest = 0.5f * Accel * T * T;
	const float WalkCap = Tune.BaseWalkSpeed * T;
	return FMath::Min(FromRest, WalkCap);
}

float CLNavAbility::RecastJumpLaunchPlaneInterceptCm(float JumpLength, float JumpHeight, float JumpMaxDepth)
{
	const float L = FMath::Max(1.f, JumpLength);
	const float H = FMath::Max(1.f, JumpHeight);
	const float D = FMath::Max(1.f, JumpMaxDepth);
	const float Root = FMath::Sqrt(H * (H + D));
	const float Num = 2.f * H + 2.f * Root;
	return L * (Num / (D + Num));
}

float CLNavAbility::AirDivePadPlaceChordCm(const FCLMovementTune& Tune)
{
	const float L = SamePlaneJumpLengthCm(Tune);
	const float H = JumpApexUpCm(Tune, FMath::Max(1, Tune.MaxJumps));
	const float D = AirDiveRefDropCm();
	return RecastJumpLaunchPlaneInterceptCm(L, H, D);
}

float CLNavAbility::MinDropCmForDistXY(const FCLMovementTune& Tune, float DistXY)
{
	const float MaxXY = FMath::Max(1.f, Tune.AirDiveMaxXY);
	const float HangT = FMath::Max(0.f, Tune.DiveHangSeconds);
	const float G = FMath::Max(1.f, Tune.GravityZ);
	const float HangG = FMath::Max(0.01f, Tune.DiveHangGravity) * G;
	const float HangFall = 0.5f * HangG * HangT * HangT;
	const float JumpXY = JumpSteerXY(Tune, FMath::Max(1, Tune.MaxJumps));
	const float NeedDiveXY = FMath::Max(0.f, DistXY - JumpXY);
	const float HangXY = MaxXY * HangT;
	if (NeedDiveXY <= HangXY + 40.f)
	{
		return HangFall + 80.f;
	}
	const float FallT = FMath::Max(0.f, NeedDiveXY / MaxXY - HangT);
	return HangFall + 0.5f * G * FallT * FallT + 40.f;
}

int32 CLNavAbility::JumpsToLaunch(const FCLMovementTune& Tune, const FVector& From, const FVector& To)
{
	FCLLaunchRecipe Recipe;
	if (LookupLaunchRecipe(Tune, CLNavTune::Get(), From, To, Recipe) && Recipe.Jumps > 0)
	{
		return Recipe.Jumps;
	}
	const float DistXY = FVector::Dist2D(From, To);
	const float NeedDrop = MinDropCmForDistXY(Tune, DistXY);
	const float TargetZ = FMath::Max(To.Z + 80.f, To.Z + NeedDrop);
	const float NeedUp = TargetZ - From.Z;
	if (NeedUp <= 30.f)
	{
		return 1;
	}
	const int32 MaxJ = FMath::Max(1, Tune.MaxJumps);
	for (int32 J = 1; J <= MaxJ; ++J)
	{
		if (JumpApexUpCm(Tune, J) >= NeedUp)
		{
			return J;
		}
	}
	return MaxJ;
}

float CLNavAbility::HangReachXY(const FCLMovementTune& Tune)
{
	return FMath::Max(0.f, Tune.AirDiveMaxXY) * FMath::Max(0.f, Tune.DiveHangSeconds);
}

bool CLNavAbility::JumpToInEnvelope(const FCLMovementTune& Tune, const FVector& From, const FVector& To)
{
	return InEnvelope(JumpTo(Tune, Tune.MaxJumps), From, To);
}

bool CLNavAbility::ReadyToAirDive(const FCLMovementTune& Tune, float MaxAccel, float SpeedXY, const FVector& From, const FVector& To)
{
	if (From.Z <= To.Z + 40.f)
	{
		return false;
	}
	FCLLaunchRecipe Recipe;
	if (!LookupLaunchRecipe(Tune, CLNavTune::Get(), From, To, Recipe))
	{
		return false;
	}
	(void)MaxAccel;
	(void)SpeedXY;
	const FCLNavAbilityBox Box = AirDiveTo(Tune, MaxAccel, SpeedXY, To.Z - From.Z);
	return InEnvelope(Box, From, To);
}

float CLNavAbility::SearchRadiusCm(const FCLMovementTune& Tune, const FCLNavTune& NavTune, float DropCm)
{
	const float Phys = MaxLaunchXY(Tune, DropCm);
	if (NavTune.AirDiveSearchMaxCm > 0.f)
	{
		return FMath::Min(Phys, NavTune.AirDiveSearchMaxCm);
	}
	return Phys;
}

FCLNavAbilityBox CLNavAbility::JumpTo(const FCLMovementTune& Tune, int32 JumpsUsed)
{
	FCLNavAbilityBox Box;
	const int32 J = FMath::Clamp(JumpsUsed, 0, FMath::Max(1, Tune.MaxJumps));
	const float Apex = JumpApexUpCm(Tune, J);
	if (J <= 0)
	{
		Box.MinDeltaZ = -1.0e7f;
		Box.MaxDeltaZ = 30.f;
		Box.MaxDistXY = 280.f;
	}
	else
	{
		Box.MinDeltaZ = -80.f;
		Box.MaxDeltaZ = Apex + 40.f;
		Box.MaxDistXY = 80.f + 120.f * static_cast<float>(J);
	}
	Box.ReleaseDistXY = 0.f;
	Box.CoastXY = 0.f;
	return Box;
}

float CLNavAbility::ReleaseDistXY(const FCLNavAbilityBox& Box, float LandingRadius)
{
	return FMath::Max(40.f, LandingRadius - Box.CoastXY);
}

FCLNavAbilityBox CLNavAbility::AirDiveTo(const FCLMovementTune& Tune, float MaxAccel, float SpeedXY, float DeltaZ)
{
	FCLNavAbilityBox Box;
	const float Drop = FMath::Max(0.f, -DeltaZ);
	Box.MinDistXY = 0.f;
	Box.MaxDistXY = SearchRadiusCm(Tune, CLNavTune::Get(), Drop) + 80.f;
	Box.MinDeltaZ = -1.0e7f;
	Box.MaxDeltaZ = JumpApexUpCm(Tune, FMath::Max(1, Tune.MaxJumps)) + 80.f;
	const float Brake = MaxAccel * FMath::Clamp(Tune.AirDiveXYBrake, 0.f, 8.f);
	Box.CoastXY = FMath::Clamp(CoastAfterReleaseCm(SpeedXY, Brake), 25.f, 120.f);
	Box.ReleaseDistXY = CLNavAbility::ReleaseDistXY(Box, 180.f);
	return Box;
}

FCLNavAbilityBox CLNavAbility::SlideTo(const FCLMovementTune& Tune)
{
	FCLNavAbilityBox Box;
	const float Sprint = SprintCmPerSec(Tune);
	const float Peak = Sprint * Tune.SlidePeakMultiplier;
	const float End = Sprint * Tune.SlideEndMultiplier;
	const float Dur = FMath::Max(0.05f, Tune.SlideDuration);
	const float AccelEnd = FMath::Clamp(Tune.SlideAccelPortion, 0.05f, 0.95f);
	const float T1 = Dur * AccelEnd;
	const float T2 = Dur - T1;
	const float Travel = 0.5f * (Sprint + Peak) * T1 + 0.5f * (Peak + End) * T2;
	Box.MinDistXY = Travel * 0.45f;
	Box.MaxDistXY = Travel * 1.15f;
	Box.MinDeltaZ = -40.f;
	Box.MaxDeltaZ = 40.f;
	Box.ReleaseDistXY = 0.f;
	Box.CoastXY = 30.f;
	return Box;
}

FCLNavAbilityBox CLNavAbility::DashTo(const FCLMovementTune& Tune)
{
	FCLNavAbilityBox Box;
	Box.MinDistXY = Tune.DashDistance * 0.7f;
	Box.MaxDistXY = Tune.DashDistance * 1.15f;
	Box.MinDeltaZ = -50.f;
	Box.MaxDeltaZ = Tune.DashHopZ + 40.f;
	return Box;
}

FCLNavAbilityBox CLNavAbility::DodgeTo(const FCLMovementTune& Tune)
{
	FCLNavAbilityBox Box;
	const float Dist = Tune.DodgeDistance * Tune.DodgeForwardScale;
	Box.MinDistXY = Dist * 0.65f;
	Box.MaxDistXY = Tune.DodgeDistance * 1.1f;
	Box.MinDeltaZ = -40.f;
	Box.MaxDeltaZ = 40.f;
	return Box;
}

bool CLNavAbility::InEnvelope(const FCLNavAbilityBox& Box, const FVector& From, const FVector& To)
{
	const float DistXY = FVector::Dist2D(From, To);
	const float DeltaZ = To.Z - From.Z;
	return DistXY >= Box.MinDistXY * 0.5f && DistXY <= Box.MaxDistXY + 80.f
		&& DeltaZ >= Box.MinDeltaZ && DeltaZ <= Box.MaxDeltaZ;
}

bool CLNavAbility::ShouldReleaseStick(const FCLNavAbilityBox& Box, float DistXY)
{
	return DistXY <= Box.ReleaseDistXY;
}

bool CLNavAbility::ShouldAbandon(const FCLNavAbilityBox& Box, const FVector& From, const FVector& To, bool bOnGround, bool bMechanicSeen)
{
	const float DistXY = FVector::Dist2D(From, To);
	const float DeltaZ = To.Z - From.Z;
	if (DistXY > Box.MaxDistXY)
	{
		return true;
	}
	if (DeltaZ > Box.MaxDeltaZ || DeltaZ < Box.MinDeltaZ)
	{
		return true;
	}
	if (bMechanicSeen && bOnGround)
	{
		return DistXY > Box.ReleaseDistXY + Box.CoastXY + 80.f;
	}
	return false;
}

bool CLNavAbility::StandingOnGoalFloor(const FVector& Loc, const FVector& GoalFloor)
{
	const float Stand = Loc.Z - GoalFloor.Z;
	return Stand >= 40.f && Stand <= 220.f;
}

bool CLNavAbility::LaunchInEnvelope(const FCLMovementTune& Tune, const FCLNavTune& NavTune, const FVector& From, const FVector& To)
{
	const float DistXY = FVector::Dist2D(From, To);
	const float DeltaZ = To.Z - From.Z;
	const float Drop = FMath::Max(0.f, -DeltaZ);
	const FCLNavAbilityBox Jump = JumpTo(Tune, Tune.MaxJumps);
	const FCLNavAbilityBox Slide = SlideTo(Tune);
	if (DistXY <= Jump.MaxDistXY && DeltaZ <= Jump.MaxDeltaZ && DeltaZ >= Jump.MinDeltaZ && Drop < 120.f)
	{
		return false;
	}
	if (DistXY <= Slide.MaxDistXY && FMath::Abs(DeltaZ) <= 40.f)
	{
		return false;
	}
	FCLLaunchRecipe Recipe;
	if (!LookupLaunchRecipe(Tune, NavTune, From, To, Recipe))
	{
		return false;
	}
	return DistXY > 450.f || Drop > 120.f;
}

bool CLNavAbility::IsAirDivePathSegment(const FCLMovementTune& Tune, const FCLNavTune& NavTune, const FVector& From, const FVector& To)
{
	const float DistXY = FVector::Dist2D(From, To);
	const float DeltaZ = To.Z - From.Z;
	if (DistXY < 450.f || DeltaZ > -80.f)
	{
		return false;
	}
	return LaunchInEnvelope(Tune, NavTune, From, To);
}

bool CLNavAbility::IsAirDiveArea(const UClass* AreaClass)
{
	return AreaClass == UCLNavArea_AirDive::StaticClass();
}

bool CLNavAbility::LookupLaunchRecipe(const FCLMovementTune& Tune, const FCLNavTune& NavTune, const FVector& From,
	const FVector& To, FCLLaunchRecipe& Out)
{
	Out = FCLLaunchRecipe();
	const float DistXY = FVector::Dist2D(From, To);
	const float DeltaZ = To.Z - From.Z;
	const float Drop = FMath::Max(0.f, -DeltaZ);
	const float Apex = JumpApexUpCm(Tune, FMath::Max(1, Tune.MaxJumps));
	if (DeltaZ > Apex + 80.f)
	{
		return false;
	}
	const float HullXY = SearchRadiusCm(Tune, NavTune, Drop);
	if (DistXY > HullXY + 80.f)
	{
		return false;
	}

	const float Hang = HangReachXY(Tune);
	const float A1 = JumpApexUpCm(Tune, 1);
	const float A2 = JumpApexUpCm(Tune, 2);
	const float A3 = JumpApexUpCm(Tune, 3);
	const float Strain = AirDiveRefDropCm();
	const int32 MaxJ = FMath::Max(1, Tune.MaxJumps);

	struct FSlice
	{
		float Z = 0.f;
		int32 J = 0;
		const TCHAR* Name = TEXT("stand");
	};
	const FSlice Slices[] = {
		{ 0.f, 0, TEXT("stand") },
		{ A1, 1, TEXT("jump1") },
		{ A2, 2, TEXT("jump2") },
		{ A3, 3, TEXT("jump3") },
		{ -A1, 1, TEXT("drop1") },
		{ -A2, 2, TEXT("drop2") },
		{ -A3, 3, TEXT("drop3") },
		{ -Strain, MaxJ, TEXT("strain") },
	};

	int32 Best = 0;
	float BestAbs = FMath::Abs(DeltaZ - Slices[0].Z);
	for (int32 i = 1; i < UE_ARRAY_COUNT(Slices); ++i)
	{
		const float Abs = FMath::Abs(DeltaZ - Slices[i].Z);
		if (Abs < BestAbs)
		{
			BestAbs = Abs;
			Best = i;
		}
	}
	if (FMath::Abs(DeltaZ) <= 80.f && DistXY > Hang)
	{
		Best = 3;
	}

	const FSlice& Slice = Slices[Best];
	const float InnerMax = FMath::Min(Hang, 220.f);
	ECLLaunchRing Ring = ECLLaunchRing::Inner;
	const TCHAR* RingName = TEXT("inner");
	float RingMin = 0.f;
	float RingMax = InnerMax;
	if (DistXY > Hang * 2.f)
	{
		Ring = ECLLaunchRing::Outer;
		RingName = TEXT("outer");
		RingMin = Hang * 2.f;
		RingMax = HullXY;
	}
	else if (DistXY > InnerMax)
	{
		Ring = ECLLaunchRing::Mid;
		RingName = TEXT("mid");
		RingMin = InnerMax;
		RingMax = Hang * 2.f;
	}

	if (Slice.J <= 0 && Ring != ECLLaunchRing::Inner)
	{
		return false;
	}

	int32 Jumps = Slice.J;
	const float NeedDrop = MinDropCmForDistXY(Tune, DistXY);
	const float NeedUp = FMath::Max(To.Z + 80.f, To.Z + NeedDrop) - From.Z;
	if (NeedUp > 30.f)
	{
		for (int32 J = 1; J <= MaxJ; ++J)
		{
			if (JumpApexUpCm(Tune, J) >= NeedUp)
			{
				Jumps = FMath::Max(Jumps, J);
				break;
			}
		}
		Jumps = FMath::Max(Jumps, 1);
	}
	if ((Drop > 120.f || DistXY > 450.f) && Jumps < 1)
	{
		Jumps = 1;
	}

	Out.bValid = true;
	Out.Jumps = Jumps;
	Out.SliceJumps = Slice.J;
	Out.Ring = Ring;
	Out.SliceZCm = Slice.Z;
	Out.RingMinXY = RingMin;
	Out.RingMaxXY = RingMax;
	Out.bJumpStill = Ring == ECLLaunchRing::Inner;
	Out.bPinUntilLand = Ring == ECLLaunchRing::Outer;
	Out.bShortPin = Ring == ECLLaunchRing::Mid;
	Out.AirSteerMul = Ring == ECLLaunchRing::Inner ? 0.f : Tune.AirDiveSteer;
	Out.SliceName = Slice.Name;
	Out.RingName = RingName;
	return true;
}
