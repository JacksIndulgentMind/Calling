#include "Game/CLAgentGotoDriver.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Nav/CLAgentNavProbe.h"
#include "Nav/CLNavPathUtil.h"
#include "Nav/CLNavTune.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "AI/CLBotBookTrace.h"
#include "Input/CLAgentIntent.h"
#include "Core/CLLog.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr float GotoWaypointRadius = 120.f;
	constexpr float GotoArriveRadius = 150.f;
	constexpr float HeadshotPitch = 0.f;
	constexpr int32 MaxGotoRepaths = 6;

	TArray<FVector> CourtPathForDest(const TArray<FVector>& Pts, const TArray<uint8>& Dive, const FVector& Dest,
		TArray<uint8>& OutDive)
	{
		TArray<FVector> Out;
		Out.Reserve(Pts.Num());
		OutDive.Reset();
		OutDive.Reserve(Pts.Num());
		for (int32 i = 0; i < Pts.Num(); ++i)
		{
			if (Pts[i].Z >= Dest.Z - 800.f)
			{
				Out.Add(Pts[i]);
				OutDive.Add(Dive.IsValidIndex(i) ? Dive[i] : 0);
			}
		}
		return Out;
	}

	bool PointIsAirDive(ARecastNavMesh* Recast, const FNavPathPoint& Pt)
	{
		if (!Recast)
		{
			return false;
		}
		const FNavMeshNodeFlags Flags(Pt.Flags);
		if (CLNavAbility::IsAirDiveArea(Recast->GetAreaClass(Flags.Area)))
		{
			return true;
		}
		if (Pt.NodeRef != INVALID_NAVNODEREF)
		{
			const uint32 AreaId = Recast->GetPolyAreaID(Pt.NodeRef);
			return CLNavAbility::IsAirDiveArea(Recast->GetAreaClass(static_cast<int32>(AreaId)));
		}
		return false;
	}

	void FillRecastPath(UNavigationPath* NavPath, ARecastNavMesh* Recast, TArray<FVector>& OutPts, TArray<uint8>& OutDive)
	{
		OutPts.Reset();
		OutDive.Reset();
		const FNavigationPath* Native = NavPath ? NavPath->GetPath().Get() : nullptr;
		const TArray<FNavPathPoint>* MeshPts = Native ? &Native->GetPathPoints() : nullptr;
		if (MeshPts && MeshPts->Num() >= 2)
		{
			OutPts.Reserve(MeshPts->Num());
			OutDive.Reserve(MeshPts->Num());
			for (int32 i = 0; i < MeshPts->Num(); ++i)
			{
				OutPts.Add((*MeshPts)[i].Location);
				uint8 Dive = 0;
				if (i > 0)
				{
					const FNavPathPoint& A = (*MeshPts)[i - 1];
					const FNavPathPoint& B = (*MeshPts)[i];
					const FNavMeshNodeFlags FA(A.Flags);
					const FNavMeshNodeFlags FB(B.Flags);
					const float DistXY = FVector::Dist2D(A.Location, B.Location);
					const bool bLink = FA.IsNavLink() || FB.IsNavLink();
					const bool bArea = PointIsAirDive(Recast, A) || PointIsAirDive(Recast, B);
					if (bArea && (bLink || DistXY > 400.f))
					{
						Dive = 1;
					}
				}
				OutDive.Add(Dive);
			}
			return;
		}
		if (NavPath && NavPath->PathPoints.Num() >= 2)
		{
			OutPts = NavPath->PathPoints;
			OutDive.SetNumZeroed(OutPts.Num());
		}
	}

	void BeginFlightIfNeeded(FCLAgentGotoDriver& D, ACLPlayerCharacter* Char)
	{
		if (D.bFlight || !Char)
		{
			return;
		}
		if (!D.Path.IsValidIndex(D.Index) || !D.PathAirDive.IsValidIndex(D.Index) || D.PathAirDive[D.Index] == 0)
		{
			return;
		}
		const FVector From = Char->GetActorLocation();
		const FVector To = D.Path[D.Index];
		D.bFlight = true;
		D.Flight.Mode = ECLNavAbilityExecMode::Launch;
		D.Flight.Goal = To;
		D.Flight.LandRadius = 150.f;
		CLBotBookTrace::GotoArm(TEXT("recastAirDive"), From, To, D.bPartial, D.Path.Num());
		D.Flight.Start(Char);
	}

	void BeginFallbackExec(FCLAgentGotoDriver& D, ACLPlayerCharacter* Char, ECLNavAbilityExecMode Mode, const TCHAR* Arm)
	{
		D.bFlight = true;
		D.Flight.Mode = Mode;
		D.Flight.Goal = D.Goal;
		D.Flight.LandRadius = 150.f;
		CLBotBookTrace::GotoArm(Arm, Char->GetActorLocation(), D.Goal, D.bPartial, D.Path.Num());
		D.Flight.Start(Char);
	}
}

void FCLAgentGotoDriver::Cancel()
{
	bActive = false;
	bPartial = false;
	bFlight = false;
	Path.Reset();
	PathAirDive.Reset();
	Index = 0;
	Goal = FVector::ZeroVector;
	JumpCooldown = 0.f;
	StuckSeconds = 0.f;
	LastWpDist = -1.f;
	Flight.Reset();
}

bool FCLAgentGotoDriver::Start(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError, bool bFromRepath)
{
	if (!Char || !World)
	{
		OutError = TEXT("no_local_pawn");
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
	const FVector From = Char->GetActorLocation();
	const bool bJump = Move && CLNavAbility::JumpToInEnvelope(Move->GetTune(), From, Dest);
	const bool bLaunch = Move && CLNavAbility::LaunchInEnvelope(Move->GetTune(), CLNavTune::Get(), From, Dest);

	FNavLocation ProjectedStart;
	FNavLocation ProjectedGoal;
	const FVector QueryExtent(800.f, 800.f, 2500.f);
	FVector StartLoc = Char->GetNavAgentLocation();
	FVector GoalLoc = Dest;
	bool bStartOk = false;
	bool bGoalOk = false;
	if (NavSys)
	{
		if (NavSys->ProjectPointToNavigation(StartLoc, ProjectedStart, QueryExtent))
		{
			StartLoc = ProjectedStart.Location;
			bStartOk = true;
		}
		if (NavSys->ProjectPointToNavigation(Dest, ProjectedGoal, QueryExtent))
		{
			GoalLoc = ProjectedGoal.Location;
			bGoalOk = true;
		}
	}

	UNavigationPath* NavPath = nullptr;
	if (NavSys && bStartOk && bGoalOk)
	{
		NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(World, StartLoc, GoalLoc, Char);
	}

	const bool bRecastValid = NavPath && NavPath->IsValid() && NavPath->PathPoints.Num() >= 2;
	TArray<FVector> RecastPts;
	TArray<uint8> RecastDive;
	if (bRecastValid)
	{
		ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
		TArray<FVector> RawPts;
		TArray<uint8> RawDive;
		FillRecastPath(NavPath, Recast, RawPts, RawDive);
		RecastPts = CourtPathForDest(RawPts, RawDive, Dest, RecastDive);
	}
	const bool bRecastComplete = RecastPts.Num() >= 2 && !NavPath->IsPartial()
		&& CLNavPathUtil::PathReachesDest(RecastPts, Dest);
	if (!bRecastComplete && !bJump && !bLaunch)
	{
		if (!NavSys)
		{
			OutError = TEXT("no_nav");
		}
		else if (!bStartOk)
		{
			OutError = TEXT("no_project_start");
		}
		else if (!bGoalOk)
		{
			OutError = TEXT("no_project_goal");
		}
		else
		{
			OutError = FString::Printf(TEXT("no_path tiles=%d"), CLAgentNavProbe::NavTileCount(World));
		}
		return false;
	}

	const int32 RepathBudget = bFromRepath ? RepathLeft : MaxGotoRepaths;
	Cancel();
	RepathLeft = RepathBudget;
	if (bRecastComplete)
	{
		Path = RecastPts;
		PathAirDive = RecastDive;
		Goal = GoalLoc;
		bPartial = false;
	}
	else
	{
		Path = { From, Dest };
		PathAirDive = { 0, 0 };
		Goal = Dest;
		bPartial = bRecastValid && NavPath->IsPartial();
	}
	Index = 1;
	bActive = true;

	if (Path.IsValidIndex(Index))
	{
		const FVector ToFirst = (Path[Index] - Char->GetActorLocation()).GetSafeNormal2D();
		if (!ToFirst.IsNearlyZero())
		{
			Char->SetLookGoalYawPitch(true, ToFirst.Rotation().Yaw, true, HeadshotPitch);
		}
	}

	if (bRecastComplete)
	{
		CLBotBookTrace::GotoArm(TEXT("recast"), From, Dest, false, Path.Num());
		BeginFlightIfNeeded(*this, Char);
	}
	else if (bJump)
	{
		BeginFallbackExec(*this, Char, ECLNavAbilityExecMode::JumpTo, TEXT("jumpTo"));
	}
	else
	{
		BeginFallbackExec(*this, Char, ECLNavAbilityExecMode::Launch, TEXT("launch"));
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
	if (FVector::Dist2D(Loc, Goal) <= GotoArriveRadius)
	{
		const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
		const bool bNeedLand = bFlight || (Goal.Z + 80.f < Loc.Z - 120.f);
		const bool bOnPad = CLNavAbility::StandingOnGoalFloor(Loc, Goal);
		if (!bNeedLand || (Move && Move->IsMovingOnGround() && !Move->IsDiving() && bOnPad))
		{
			Cancel();
			Char->ClearAgentIntent();
			return;
		}
	}

	BeginFlightIfNeeded(*this, Char);
	if (bFlight)
	{
		Flight.Tick(DeltaSeconds, Char);
		if (Flight.bFailed)
		{
			Cancel();
			Char->ClearAgentIntent();
			return;
		}
		if (Flight.bFinished)
		{
			Cancel();
			Char->ClearAgentIntent();
			return;
		}
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

	FVector2D MoveXY(0.f, 1.f);

	bool bJump = false;
	if (World)
	{
		const FCLNavProbeTune& Probe = CLNavTune::Get().Probe;
		const float HalfH = Char->GetCapsuleComponent() ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
		const FVector Waist = Loc - ToTarget * Probe.StartBackupCm;
		const FVector Head = Loc + FVector(0.f, 0.f, Probe.HeadLiftCm) - ToTarget * Probe.StartBackupCm;
		const FCLAgentBlockHit Fwd = CLAgentNavProbe::ProbeBlock(World, Char, Waist, ToTarget, Loc.Z - HalfH);
		const FCLAgentBlockHit HeadHit = CLAgentNavProbe::ProbeBlock(World, Char, Head, ToTarget, Loc.Z - HalfH);
		const float Drop = CLAgentNavProbe::FloorDropCm(World, Char, Loc, HalfH, ToTarget, 90.f);
		const bool bOnGround = !Char->GetCombatMovement() || Char->GetCombatMovement()->IsMovingOnGround();
		const bool bJumpUp = Fwd.Kind == ECLFwdKind::JumpUp && Fwd.Dist < Probe.JumpFaceCm;
		const bool bJumpCover = Fwd.Kind == ECLFwdKind::Cover && Fwd.Dist < Probe.JumpFaceCm && HeadHit.Dist >= Probe.JumpableHeadClearCm;
		const bool bJumpDown = Fwd.Kind == ECLFwdKind::JumpDown && Fwd.Dist < Probe.JumpFaceCm;
		const bool bHasLanding = Fwd.Kind == ECLFwdKind::Drop || Fwd.Kind == ECLFwdKind::JumpDown;
		const int32 JumpsLeft = Char->GetCombatMovement() ? Char->GetCombatMovement()->GetJumpsRemaining() : 0;
		if (Drop >= Probe.FloorProbeMaxCm && !bHasLanding && MoveXY.Y > 0.f)
		{
			MoveXY.Y = 0.f;
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
	Intent.Move = MoveXY;
	Intent.bSprint = true;
	Intent.bJump = bJump;
	Char->ApplyAgentIntent(Intent);
}
