#include "Game/CLAgentGotoDriver.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Nav/CLAgentNavProbe.h"
#include "Nav/CLNavTune.h"
#include "Input/CLAgentIntent.h"
#include "Core/CLLog.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr float GotoWaypointRadius = 120.f;
	constexpr float GotoArriveRadius = 150.f;
	constexpr float HeadshotPitch = 0.f;
	constexpr int32 MaxGotoRepaths = 6;
}

void FCLAgentGotoDriver::Cancel()
{
	bActive = false;
	bPartial = false;
	Path.Reset();
	Index = 0;
	Goal = FVector::ZeroVector;
	JumpCooldown = 0.f;
	StuckSeconds = 0.f;
	LastWpDist = -1.f;
}

bool FCLAgentGotoDriver::Start(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError, bool bFromRepath)
{
	if (!Char || !World)
	{
		OutError = TEXT("no_local_pawn");
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		OutError = TEXT("no_nav");
		return false;
	}

	FNavLocation ProjectedStart;
	FNavLocation ProjectedGoal;
	const FVector QueryExtent(800.f, 800.f, 2500.f);
	FVector StartLoc = Char->GetNavAgentLocation();
	if (NavSys->ProjectPointToNavigation(StartLoc, ProjectedStart, QueryExtent))
	{
		StartLoc = ProjectedStart.Location;
	}
	else
	{
		OutError = TEXT("no_project_start");
		return false;
	}

	FVector GoalLoc = Dest;
	if (NavSys->ProjectPointToNavigation(Dest, ProjectedGoal, QueryExtent))
	{
		GoalLoc = ProjectedGoal.Location;
	}
	else
	{
		OutError = TEXT("no_project_goal");
		return false;
	}

	UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, StartLoc, GoalLoc, Char);
	if (!NavPath || !NavPath->IsValid() || NavPath->PathPoints.Num() < 2)
	{
		OutError = FString::Printf(TEXT("no_path tiles=%d"), CLAgentNavProbe::NavTileCount(World));
		return false;
	}

	const int32 RepathBudget = bFromRepath ? RepathLeft : MaxGotoRepaths;
	Cancel();
	RepathLeft = RepathBudget;
	Path = NavPath->PathPoints;
	Index = 1;
	Goal = GoalLoc;
	bPartial = NavPath->IsPartial();
	bActive = true;

	if (Path.IsValidIndex(Index))
	{
		const FVector ToFirst = (Path[Index] - Char->GetActorLocation()).GetSafeNormal2D();
		if (!ToFirst.IsNearlyZero())
		{
			Char->SetLookGoalYawPitch(true, ToFirst.Rotation().Yaw, true, HeadshotPitch);
		}
	}
	return true;
}

void FCLAgentGotoDriver::Tick(float DeltaSeconds, UWorld* World, ACLPlayerCharacter* Char)
{
	if (!bActive || Path.Num() == 0 || !Char)
	{
		return;
	}

	JumpCooldown = FMath::Max(0.f, JumpCooldown - DeltaSeconds);

	const FVector Loc = Char->GetActorLocation();
	if (FVector::Dist2D(Loc, Goal) <= GotoArriveRadius && FMath::Abs(Loc.Z - Goal.Z) < 250.f)
	{
		Cancel();
		Char->ClearAgentIntent();
		return;
	}

	while (Path.IsValidIndex(Index) && FVector::Dist2D(Loc, Path[Index]) <= GotoWaypointRadius)
	{
		++Index;
		StuckSeconds = 0.f;
		LastWpDist = -1.f;
	}
	if (!Path.IsValidIndex(Index))
	{
		if (RepathLeft > 0)
		{
			--RepathLeft;
			FString Error;
			if (Start(World, Char, Goal, Error, true) && Path.IsValidIndex(Index))
			{
				return;
			}
		}
	}

	const FVector SteerAt = Path.IsValidIndex(Index) ? Path[Index] : Goal;
	const FVector ToTarget = (SteerAt - Loc).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const float WpDist = FVector::Dist2D(Loc, Path.IsValidIndex(Index) ? Path[Index] : Goal);
	if (LastWpDist < 0.f || WpDist < LastWpDist - 15.f)
	{
		StuckSeconds = 0.f;
		LastWpDist = WpDist;
	}
	else
	{
		StuckSeconds += DeltaSeconds;
		LastWpDist = FMath::Min(LastWpDist, WpDist);
	}

	Char->SetLookGoalYawPitch(true, ToTarget.Rotation().Yaw, true, HeadshotPitch);

	FVector2D Move(0.f, 1.f);

	bool bJump = false;
	if (World)
	{
		const FCLNavProbeTune& Probe = CLNavTune::Get().Probe;
		const float HalfH = Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
		const float FeetZ = Loc.Z - HalfH;
		const FVector Waist = Loc - ToTarget * Probe.StartBackupCm;
		const FVector Head = Loc + FVector(0.f, 0.f, Probe.HeadLiftCm) - ToTarget * Probe.StartBackupCm;
		const FCLAgentBlockHit Fwd = CLAgentNavProbe::ProbeBlock(World, Char, Waist, ToTarget, FeetZ);
		const FCLAgentBlockHit HeadHit = CLAgentNavProbe::ProbeBlock(World, Char, Head, ToTarget, FeetZ);
		const float Drop = CLAgentNavProbe::FloorDropCm(World, Char, Loc, HalfH, ToTarget, 90.f);
		const bool bOnGround = !Char->GetCombatMovement() || Char->GetCombatMovement()->IsMovingOnGround();
		const bool bJumpUp = Fwd.Kind == ECLFwdKind::JumpUp && Fwd.Dist < Probe.JumpFaceCm;
		const bool bJumpCover = Fwd.Kind == ECLFwdKind::Cover && Fwd.Dist < Probe.JumpFaceCm && HeadHit.Dist >= Probe.JumpableHeadClearCm;
		const bool bJumpDown = Fwd.Kind == ECLFwdKind::JumpDown && Fwd.Dist < Probe.JumpFaceCm;
		const bool bHasLanding = Fwd.Kind == ECLFwdKind::Drop || Fwd.Kind == ECLFwdKind::JumpDown;
		const int32 JumpsLeft = Char->GetCombatMovement() ? Char->GetCombatMovement()->GetJumpsRemaining() : 0;
		if (Drop >= Probe.FloorProbeMaxCm && !bHasLanding && Move.Y > 0.f)
		{
			Move.Y = 0.f;
		}
		if (JumpCooldown <= 0.f && (bJumpCover || bJumpDown || bJumpUp)
			&& (bOnGround || (bJumpUp && JumpsLeft > 0)))
		{
			bJump = true;
			JumpCooldown = 0.45f;
			StuckSeconds = 0.f;
		}
	}

	FCLAgentIntent Intent;
	Intent.Move = Move;
	Intent.bSprint = true;
	Intent.bJump = bJump;
	Char->ApplyAgentIntent(Intent);
}
