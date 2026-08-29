#include "Nav/CLNavAbilityExec.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Nav/CLNavTune.h"
#include "AI/CLBotBookTrace.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Input/CLAgentIntent.h"
#include "Core/CLLog.h"

namespace
{
	constexpr float SampleHz = 0.25f;
	constexpr float StrongUpVz = 120.f;
}

const TCHAR* FCLNavAbilityExec::ModeLabel() const
{
	switch (Mode)
	{
	case ECLNavAbilityExecMode::JumpTo: return TEXT("jumpTo");
	case ECLNavAbilityExecMode::AirDiveTo: return TEXT("airDiveTo");
	case ECLNavAbilityExecMode::Launch: return TEXT("launch");
	case ECLNavAbilityExecMode::SlideTo: return TEXT("slideTo");
	case ECLNavAbilityExecMode::DashTo: return TEXT("dashTo");
	case ECLNavAbilityExecMode::DodgeTo: return TEXT("dodgeTo");
	default: return TEXT("unknown");
	}
}

void FCLNavAbilityExec::Reset()
{
	Fired = 0;
	Acc = 0.f;
	Elapsed = 0.f;
	bDiveSeen = false;
	bDivePhase = false;
	bFinished = false;
	bFailed = false;
	OffPadRetries = 0;
	bStickHeld = false;
	bPinUntilLand = false;
	Recipe = FCLLaunchRecipe();
	TraceSub.Reset();
	bSlideLatched = false;
	bBurstFired = false;
	bBurstSeen = false;
	bLoggedStart = false;
	SampleAcc = 0.f;
	PhaseName = bDivePhase ? TEXT("dive") : TEXT("start");
	VelXYMin = VelXYMax = VelXYSum = 0.f;
	VelZMin = VelZMax = 0.f;
	VelN = 0;
}

void FCLNavAbilityExec::SetPhase(const TCHAR* Name, ACLPlayerCharacter* Char, const TCHAR* Extra)
{
	if (PhaseName == Name)
	{
		return;
	}
	FlushVel();
	PhaseName = Name;
	if (!Char)
	{
		return;
	}
	CLBotBookTrace::Phase(ModeLabel(), Name, Elapsed, Char->GetActorLocation(), Goal, Char->GetVelocity(), Extra);
}

void FCLNavAbilityExec::AccelVel(const FVector& Vel)
{
	const float XY = Vel.Size2D();
	if (VelN == 0)
	{
		VelXYMin = VelXYMax = XY;
		VelZMin = VelZMax = Vel.Z;
	}
	else
	{
		VelXYMin = FMath::Min(VelXYMin, XY);
		VelXYMax = FMath::Max(VelXYMax, XY);
		VelZMin = FMath::Min(VelZMin, Vel.Z);
		VelZMax = FMath::Max(VelZMax, Vel.Z);
	}
	VelXYSum += XY;
	++VelN;
}

void FCLNavAbilityExec::FlushVel()
{
	if (VelN <= 0)
	{
		return;
	}
	CLBotBookTrace::VelInterval(ModeLabel(), *PhaseName, VelXYMin, VelXYMax, VelXYSum / static_cast<float>(VelN),
		VelZMin, VelZMax);
	VelN = 0;
	VelXYSum = 0.f;
}

void FCLNavAbilityExec::LogStartIfNeeded(ACLPlayerCharacter* Char, const FCLNavAbilityBox& Box, const TCHAR* Sub, int32 Jumps)
{
	if (bLoggedStart || !Char)
	{
		return;
	}
	bLoggedStart = true;
	CLBotBookTrace::ExecStart(ModeLabel(), Char->GetActorLocation(), Goal, Box, Sub, Jumps);
}

void FCLNavAbilityExec::LogMiss(ACLPlayerCharacter* Char, const FCLNavAbilityBox& Box, const TCHAR* Result)
{
	if (!Char)
	{
		return;
	}
	const bool bOnPad = CLNavAbility::StandingOnGoalFloor(Char->GetActorLocation(), Goal);
	CLBotBookTrace::Miss(ModeLabel(), Result, *PhaseName, Char->GetActorLocation(), Goal, Box, Box.ReleaseDistXY, bOnPad);
}

FCLNavAbilityBox FCLNavAbilityExec::BoxFor(ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move) const
{
	const FCLMovementTune& Tune = Move->GetTune();
	const FVector Loc = Char->GetActorLocation();
	const float SpeedXY = Char->GetVelocity().Size2D();
	switch (Mode)
	{
	case ECLNavAbilityExecMode::JumpTo:
		return CLNavAbility::JumpTo(Tune, Tune.MaxJumps);
	case ECLNavAbilityExecMode::SlideTo:
		return CLNavAbility::SlideTo(Tune);
	case ECLNavAbilityExecMode::DashTo:
		return CLNavAbility::DashTo(Tune);
	case ECLNavAbilityExecMode::DodgeTo:
		return CLNavAbility::DodgeTo(Tune);
	default:
		return CLNavAbility::AirDiveTo(Tune, Move->GetMaxAcceleration(), SpeedXY, Goal.Z - Loc.Z);
	}
}

void FCLNavAbilityExec::FaceGoal(ACLPlayerCharacter* Char, const FVector& Loc) const
{
	const FVector To = (Goal - Loc).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		Char->SetLookGoalYawPitch(true, To.Rotation().Yaw, true, 0.f);
	}
}

bool FCLNavAbilityExec::FacingGoal(ACLPlayerCharacter* Char, const FVector& Loc) const
{
	const FVector To = (Goal - Loc).GetSafeNormal2D();
	if (To.IsNearlyZero())
	{
		return true;
	}
	const float Want = To.Rotation().Yaw;
	const float Cur = Char->GetControlRotation().Yaw;
	return FMath::Abs(FRotator::NormalizeAxis(Want - Cur)) <= 18.f;
}

void FCLNavAbilityExec::Start(ACLPlayerCharacter* Char)
{
	Reset();
	if (!Char)
	{
		bFailed = true;
		return;
	}
	UCLCombatMovementComponent* Move = Char->GetCombatMovement();
	if (!Move)
	{
		bFailed = true;
		return;
	}
	const FCLMovementTune& Tune = Move->GetTune();
	if (Mode == ECLNavAbilityExecMode::Launch || Mode == ECLNavAbilityExecMode::AirDiveTo)
	{
		if (CLNavAbility::LookupLaunchRecipe(Tune, CLNavTune::Get(), Char->GetActorLocation(), Goal, Recipe) && Recipe.bValid)
		{
			JumpPulses = FMath::Max(1, Recipe.Jumps);
			bPinUntilLand = Recipe.bPinUntilLand;
			Move->SetDivePinGravity(Recipe.bPinUntilLand || Recipe.bShortPin);
			Move->SetDiveAirSteer(Recipe.AirSteerMul);
			TraceSub = FString::Printf(TEXT("slice=%s ring=%s"), Recipe.SliceName, Recipe.RingName);
		}
		else
		{
			// Recast goto may own a Launch hop that LookupLaunchRecipe rejects (preferJump /
			// flat chord). Still fly — refusing leaves goto stuck on a Launch-owned step.
			const float DistXY = FVector::Dist2D(Char->GetActorLocation(), Goal);
			const bool bLong = DistXY > CLNavAbility::HangReachXY(Tune);
			JumpPulses = FMath::Max(1, CLNavAbility::JumpsToLaunch(Tune, Char->GetActorLocation(), Goal));
			bPinUntilLand = bLong;
			Move->SetDivePinGravity(bLong);
			Move->SetDiveAirSteer(bLong ? Tune.AirDiveSteer : -1.f);
			TraceSub = Mode == ECLNavAbilityExecMode::Launch
				? TEXT("recastLaunchFallback")
				: ModeLabel();
			if (Mode == ECLNavAbilityExecMode::Launch)
			{
				UE_LOG(LogCalling, Display,
					TEXT("Launch Start: noRecipe → fallback jumps=%d pin=%d distXY=%.0f"),
					JumpPulses, bPinUntilLand ? 1 : 0, DistXY);
			}
		}
		if (!Move->IsMovingOnGround()
			&& CLNavAbility::ReadyToAirDive(Tune, Move->GetMaxAcceleration(), Char->GetVelocity().Size2D(),
				Char->GetActorLocation(), Goal))
		{
			bDivePhase = true;
			PhaseName = TEXT("dive");
		}
		else
		{
			JumpPulses = FMath::Max(1, JumpPulses);
		}
	}
	else if (Mode == ECLNavAbilityExecMode::JumpTo)
	{
		JumpPulses = FMath::Max(1, CLNavAbility::JumpsToLaunch(Tune, Char->GetActorLocation(), Goal));
		TraceSub = TEXT("jump");
	}
	else
	{
		TraceSub = ModeLabel();
	}
	const FCLNavAbilityBox Box = BoxFor(Char, Move);
	LogStartIfNeeded(Char, Box, *TraceSub, JumpPulses);
	if (Mode == ECLNavAbilityExecMode::SlideTo || Mode == ECLNavAbilityExecMode::DashTo
		|| Mode == ECLNavAbilityExecMode::DodgeTo)
	{
		if (!CLNavAbility::InEnvelope(Box, Char->GetActorLocation(), Goal))
		{
			LogMiss(Char, Box, TEXT("startOutside"));
			bFailed = true;
			return;
		}
	}
	Tick(0.f, Char);
}

void FCLNavAbilityExec::Tick(float DeltaSeconds, ACLPlayerCharacter* Char)
{
	if (bFinished || bFailed || !Char)
	{
		return;
	}
	Elapsed += DeltaSeconds;
	UCLCombatMovementComponent* Move = Char->GetCombatMovement();
	if (!Move)
	{
		bFailed = true;
		return;
	}
	AccelVel(Char->GetVelocity());
	SampleAcc += DeltaSeconds;
	if (SampleAcc >= SampleHz)
	{
		SampleAcc = 0.f;
		CLBotBookTrace::Sample(ModeLabel(), *PhaseName, Elapsed, DeltaSeconds, Char->GetActorLocation(),
			Char->GetVelocity(), Goal);
	}

	if (Mode == ECLNavAbilityExecMode::SlideTo)
	{
		TickSlide(DeltaSeconds, Char, Move);
		return;
	}
	if (Mode == ECLNavAbilityExecMode::DashTo || Mode == ECLNavAbilityExecMode::DodgeTo)
	{
		TickBurst(DeltaSeconds, Char, Move);
		return;
	}
	TickJumpLaunch(DeltaSeconds, Char, Move);
}

void FCLNavAbilityExec::TickJumpLaunch(float DeltaSeconds, ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move)
{
	if (Move->IsDiving() || Move->IsDiveReported())
	{
		bDiveSeen = true;
	}

	const FVector Loc = Char->GetActorLocation();
	const float DistXY = FVector::Dist2D(Loc, Goal);
	const float DeltaZ = Goal.Z - Loc.Z;
	const float SpeedXY = Char->GetVelocity().Size2D();
	const FCLMovementTune& Tune = Move->GetTune();
	FCLNavAbilityBox Box = CLNavAbility::AirDiveTo(Tune, Move->GetMaxAcceleration(), SpeedXY, DeltaZ);
	Box.ReleaseDistXY = CLNavAbility::ReleaseDistXY(Box, LandRadius);
	const FCLNavAbilityBox JumpBox = CLNavAbility::JumpTo(Tune, Tune.MaxJumps);
	const bool bOnGround = Move->IsMovingOnGround();
	const bool bOnPad = CLNavAbility::StandingOnGoalFloor(Loc, Goal);
	const bool bLanded = bOnGround && !Move->IsDiving() && DistXY <= LandRadius && bOnPad;
	const bool bVzOk = Char->GetVelocity().Z < StrongUpVz;
	const bool bReadyDive = !bOnGround && bVzOk
		&& CLNavAbility::ReadyToAirDive(Tune, Move->GetMaxAcceleration(), SpeedXY, Loc, Goal);

	FaceGoal(Char, Loc);
	if (!FacingGoal(Char, Loc) && !bDiveSeen)
	{
		FCLAgentIntent WaitLook;
		Char->ApplyAgentIntent(WaitLook);
		return;
	}

	if (Mode == ECLNavAbilityExecMode::JumpTo)
	{
		LogStartIfNeeded(Char, JumpBox, TEXT("jump"), JumpPulses);
		if (CLNavAbility::ShouldAbandon(JumpBox, Loc, Goal, bOnGround, Fired > 0) && Elapsed > 0.4f)
		{
			LogMiss(Char, JumpBox, TEXT("abandon"));
			bFailed = true;
			Char->ClearAgentIntent();
			return;
		}
		if (bLanded && DeltaZ <= JumpBox.MaxDeltaZ && DistXY <= FMath::Max(LandRadius, 80.f))
		{
			FlushVel();
			LogMiss(Char, JumpBox, TEXT("ok"));
			bFinished = true;
			Char->ClearAgentIntent();
			return;
		}
	}
	else if (bDivePhase)
	{
		const bool bHanging = Move->IsDiving() && Move->GetDiveElapsed() < Tune.DiveHangSeconds;
		if (Elapsed > 0.35f && !bHanging
			&& CLNavAbility::ShouldAbandon(Box, Loc, Goal, bOnGround, bDiveSeen))
		{
			LogMiss(Char, Box, TEXT("abandon"));
			bFailed = true;
			Char->ClearAgentIntent();
			return;
		}
		if (bDiveSeen && bOnGround && Elapsed > 0.4f && !bOnPad)
		{
			const bool bCanRetry = OffPadRetries < 1
				&& DistXY <= LandRadius + 400.f
				&& Loc.Z > Goal.Z - 500.f;
			if (bCanRetry)
			{
				++OffPadRetries;
				bDivePhase = false;
				bDiveSeen = false;
				Fired = 0;
				Acc = 0.f;
				SetPhase(TEXT("retry"), Char, TEXT("offPad"));
			}
			else
			{
				LogMiss(Char, Box, TEXT("offPad"));
				bFailed = true;
				Char->ClearAgentIntent();
				return;
			}
		}
		if (bLanded)
		{
			FlushVel();
			LogMiss(Char, Box, TEXT("ok"));
			bFinished = true;
			Char->ClearAgentIntent();
			return;
		}
	}
	else if (Elapsed > 2.8f && bOnGround && Fired >= JumpPulses && !bReadyDive)
	{
		LogMiss(Char, Box, TEXT("noDive"));
		bFailed = true;
		Char->ClearAgentIntent();
		return;
	}

	if (!bDivePhase && Mode != ECLNavAbilityExecMode::JumpTo && bReadyDive)
	{
		SetPhase(TEXT("dive"), Char, TEXT("activate"));
		bDivePhase = true;
	}

	FCLAgentIntent Intent;
	const FVector To = (Goal - Loc).GetSafeNormal2D();
	const bool bJumpStill = Recipe.bValid ? Recipe.bJumpStill : DistXY <= CLNavAbility::HangReachXY(Tune);
	if (bDivePhase)
	{
		if (Recipe.bShortPin && DistXY <= CLNavAbility::HangReachXY(Tune))
		{
			Move->SetDivePinGravity(false);
		}
		const bool bHanging = Move->IsDiving() && Move->GetDiveElapsed() < Tune.DiveHangSeconds;
		const bool bRelease = !Recipe.bPinUntilLand && CLNavAbility::ShouldReleaseStick(Box, DistXY);
		Intent.Move = (bRelease || To.IsNearlyZero()) ? FVector2D::ZeroVector : FVector2D(0.f, 1.f);
		Intent.bSprint = !Intent.Move.IsNearlyZero();
		Intent.bAirDive = true;
		const FString SliceRing = FString::Printf(TEXT("slice=%s ring=%s"), Recipe.SliceName, Recipe.RingName);
		if (bHanging)
		{
			SetPhase(TEXT("hang"), Char, *SliceRing);
		}
		else if (bRelease && bStickHeld)
		{
			SetPhase(TEXT("release"), Char,
				*FString::Printf(TEXT("%s distXY=%.0f release=%.0f"), *SliceRing, DistXY, Box.ReleaseDistXY));
			bStickHeld = false;
		}
		else if (!bRelease && !bStickHeld)
		{
			SetPhase(bPinUntilLand ? TEXT("pinnedSteer") : TEXT("hold"), Char, *SliceRing);
			bStickHeld = true;
		}
		else if (bPinUntilLand && bStickHeld && PhaseName != TEXT("hang"))
		{
			SetPhase(TEXT("pinnedSteer"), Char, *SliceRing);
		}
	}
	else
	{
		if (PhaseName != TEXT("retry"))
		{
			SetPhase(TEXT("jump"), Char);
		}
		Intent.Move = (bJumpStill || To.IsNearlyZero()) ? FVector2D::ZeroVector : FVector2D(0.f, 1.f);
		Intent.bSprint = !Intent.Move.IsNearlyZero();
		Acc += DeltaSeconds;
		if (Fired < JumpPulses && (Fired == 0 || Acc >= PulseGap))
		{
			Acc = 0.f;
			++Fired;
			Intent.bJump = true;
			CLBotBookTrace::Phase(ModeLabel(), TEXT("jumpPulse"), Elapsed, Loc, Goal, Char->GetVelocity(),
				*FString::Printf(TEXT("n=%d still=%d"), Fired, bJumpStill ? 1 : 0));
		}
	}
	Char->ApplyAgentIntent(Intent);
}

void FCLNavAbilityExec::TickSlide(float DeltaSeconds, ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move)
{
	(void)DeltaSeconds;
	const FVector Loc = Char->GetActorLocation();
	const FCLNavAbilityBox Box = CLNavAbility::SlideTo(Move->GetTune());
	LogStartIfNeeded(Char, Box, TEXT("slide"), 0);
	FaceGoal(Char, Loc);
	const FVector To = (Goal - Loc).GetSafeNormal2D();
	if (!bSlideLatched && !Move->CanCommitSlideInDir(To.IsNearlyZero() ? Char->GetActorForwardVector() : To))
	{
		LogMiss(Char, Box, TEXT("noFloor"));
		bFailed = true;
		Char->ClearAgentIntent();
		return;
	}

	FCLAgentIntent Intent;
	Intent.Move = To.IsNearlyZero() ? FVector2D::ZeroVector : FVector2D(0.f, 1.f);
	Intent.bSprint = true;
	if (Elapsed >= 0.18f && !Move->IsSliding() && !bSlideLatched)
	{
		Intent.bSlide = true;
		SetPhase(TEXT("slidePulse"), Char);
	}
	if (Move->IsSliding())
	{
		if (!bSlideLatched)
		{
			SetPhase(TEXT("slide"), Char, TEXT("latch"));
		}
		bSlideLatched = true;
	}
	if (bSlideLatched && !Move->IsSliding() && Elapsed > 0.4f)
	{
		const float DistXY = FVector::Dist2D(Loc, Goal);
		const bool bOnPad = CLNavAbility::StandingOnGoalFloor(Loc, Goal);
		if (Move->IsMovingOnGround() && DistXY <= FMath::Max(LandRadius, 80.f) && bOnPad)
		{
			FlushVel();
			LogMiss(Char, Box, TEXT("ok"));
			bFinished = true;
		}
		else
		{
			LogMiss(Char, Box, TEXT("miss"));
			bFailed = true;
		}
		Char->ClearAgentIntent();
		return;
	}
	Char->ApplyAgentIntent(Intent);
}

void FCLNavAbilityExec::TickBurst(float DeltaSeconds, ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move)
{
	(void)DeltaSeconds;
	const FVector Loc = Char->GetActorLocation();
	const bool bDash = Mode == ECLNavAbilityExecMode::DashTo;
	const FCLNavAbilityBox Box = bDash ? CLNavAbility::DashTo(Move->GetTune()) : CLNavAbility::DodgeTo(Move->GetTune());
	LogStartIfNeeded(Char, Box, bDash ? TEXT("dash") : TEXT("dodge"), 0);
	FaceGoal(Char, Loc);
	const bool bBusy = bDash ? Move->IsDashing() : Move->IsDodging();
	if (bBusy)
	{
		bBurstSeen = true;
		SetPhase(bDash ? TEXT("dash") : TEXT("dodge"), Char);
	}

	FCLAgentIntent Intent;
	const FVector To = (Goal - Loc).GetSafeNormal2D();
	Intent.Move = To.IsNearlyZero() ? FVector2D::ZeroVector : FVector2D(0.f, 1.f);
	if (!bBurstFired)
	{
		if (bDash) { Intent.bDash = true; }
		else { Intent.bDodge = true; }
		bBurstFired = true;
		SetPhase(TEXT("burst"), Char);
		CLBotBookTrace::Phase(ModeLabel(), TEXT("burstPulse"), Elapsed, Loc, Goal, Char->GetVelocity());
	}

	if (bBurstSeen && !bBusy && Move->IsMovingOnGround() && Elapsed > 0.12f)
	{
		const float DistXY = FVector::Dist2D(Loc, Goal);
		const bool bOnPad = CLNavAbility::StandingOnGoalFloor(Loc, Goal);
		if (DistXY <= FMath::Max(LandRadius, 80.f) && bOnPad)
		{
			FlushVel();
			LogMiss(Char, Box, TEXT("ok"));
			bFinished = true;
		}
		else
		{
			LogMiss(Char, Box, TEXT("miss"));
			bFailed = true;
		}
		Char->ClearAgentIntent();
		return;
	}
	if (Elapsed > 1.4f)
	{
		LogMiss(Char, Box, TEXT("timeout"));
		bFailed = true;
		Char->ClearAgentIntent();
		return;
	}
	Char->ApplyAgentIntent(Intent);
}

bool FCLNavAbilityExec::SuccessImpossible(ACLPlayerCharacter* Char) const
{
	if (bFailed)
	{
		return true;
	}
	if (!Char || bFinished)
	{
		return false;
	}
	if (Mode == ECLNavAbilityExecMode::SlideTo || Mode == ECLNavAbilityExecMode::DashTo
		|| Mode == ECLNavAbilityExecMode::DodgeTo || Mode == ECLNavAbilityExecMode::JumpTo)
	{
		return false;
	}
	if (!bDivePhase)
	{
		return false;
	}
	const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
	if (!Move)
	{
		return true;
	}
	const FVector Loc = Char->GetActorLocation();
	const float DistXY = FVector::Dist2D(Loc, Goal);
	const float DeltaZ = Goal.Z - Loc.Z;
	const FCLNavAbilityBox Box = CLNavAbility::AirDiveTo(
		Move->GetTune(), Move->GetMaxAcceleration(), Char->GetVelocity().Size2D(), DeltaZ);
	if (DistXY > Box.MaxDistXY || DeltaZ > Box.MaxDeltaZ)
	{
		return true;
	}
	if (Elapsed < 0.4f)
	{
		return false;
	}
	return CLNavAbility::ShouldAbandon(Box, Loc, Goal, Move->IsMovingOnGround(), bDiveSeen);
}
