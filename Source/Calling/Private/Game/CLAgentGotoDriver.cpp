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
#include "Core/CLTunes.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCallingGoto, Log, All);

namespace
{
	constexpr float GotoWaypointRadius = 120.f;
	constexpr float GotoArriveRadius = 150.f;
	constexpr float HeadshotPitch = 0.f;
	constexpr int32 MaxGotoRepaths = 6;

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

	FString AreaClassName(ARecastNavMesh* Recast, const FNavPathPoint& Pt)
	{
		if (!Recast)
		{
			return TEXT("none");
		}
		const FNavMeshNodeFlags Flags(Pt.Flags);
		if (const UClass* C = Recast->GetAreaClass(Flags.Area))
		{
			return C->GetName();
		}
		if (Pt.NodeRef != INVALID_NAVNODEREF)
		{
			const uint32 AreaId = Recast->GetPolyAreaID(Pt.NodeRef);
			if (const UClass* C = Recast->GetAreaClass(static_cast<int32>(AreaId)))
			{
				return C->GetName();
			}
		}
		return TEXT("unknown");
	}

	void LogDiveSegment(ARecastNavMesh* Recast, int32 i, const FNavPathPoint& A, const FNavPathPoint& B,
		const FVector& GotoGoal, bool bPartial, int32 PathPts)
	{
		const float SegXY = FVector::Dist2D(A.Location, B.Location);
		const float SegDZ = B.Location.Z - A.Location.Z;
		const float Seg3D = FVector::Dist(A.Location, B.Location);
		const float GoalXY = FVector::Dist2D(B.Location, GotoGoal);
		const float GoalDZ = GotoGoal.Z - B.Location.Z;
		const float FromGoalXY = FVector::Dist2D(A.Location, GotoGoal);
		float AirLen = 0.f;
		FString AirDepth = TEXT("?");
		FString AirH = TEXT("?");
		for (const FCLNavLinkTune& L : CLNavTune::Get().Links)
		{
			if (CLNavTune::IsAirDiveLink(L.Name))
			{
				AirLen = L.JumpLength;
				AirDepth = L.JumpMaxDepth;
				AirH = L.JumpHeight;
				break;
			}
		}
		FCLMovementTune MoveTune;
		MoveTune.LoadFromIni();
		FString LaunchWhy;
		const bool bLaunch = CLNavAbility::ExplainLaunchInEnvelope(MoveTune, CLNavTune::Get(), A.Location, B.Location, LaunchWhy);
		const FNavMeshNodeFlags FA(A.Flags);
		const FNavMeshNodeFlags FB(B.Flags);
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto WHY_DIVE i=%d | Recast off-mesh AirDive area on FindPath (not BotBook :airDive) | linkA=%d linkB=%d areaA=%s areaB=%s"),
			i, FA.IsNavLink() ? 1 : 0, FB.IsNavLink() ? 1 : 0, *AreaClassName(Recast, A), *AreaClassName(Recast, B));
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto WHY_DIVE i=%d | TO landing=(%.0f,%.0f,%.0f) FROM lip=(%.0f,%.0f,%.0f) | hop distXY=%.0f dZ=%.0f dist3D=%.0f"),
			i, B.Location.X, B.Location.Y, B.Location.Z, A.Location.X, A.Location.Y, A.Location.Z, SegXY, SegDZ, Seg3D);
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto WHY_DIVE i=%d | vs gotoGoal=(%.0f,%.0f,%.0f) | landing→goal distXY=%.0f dZ=%.0f | lip→goal distXY=%.0f | pathPts=%d partial=%d"),
			i, GotoGoal.X, GotoGoal.Y, GotoGoal.Z, GoalXY, GoalDZ, FromGoalXY, PathPts, bPartial ? 1 : 0);
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto WHY_DIVE i=%d | bake AirDive* JumpLength=%.0f JumpMaxDepth=%s JumpHeight=%s (look radius can invent this hop anywhere in bounds)"),
			i, AirLen, *AirDepth, *AirH);
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto WHY_DIVE i=%d | play LaunchInEnvelope %s | %s"),
			i, bLaunch ? TEXT("thinks_possible") : TEXT("thinks_impossible"), *LaunchWhy);
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto WHY_DIVE i=%d | necessary? FindPath picked this AirDive link (area DefaultCost=%.0f; walk=1 so short chords lose when cost is high) | desirable? only if that hop is the intended traverse"),
			i, CLNavTune::Get().AreaCostAirDive);
	}

	void FillRecastPath(UNavigationPath* NavPath, ARecastNavMesh* Recast, TArray<FVector>& OutPts, TArray<uint8>& OutDive,
		const FVector& GotoGoal, bool bPartial)
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
					const bool bLink = FA.IsNavLink() || FB.IsNavLink();
					const bool bArea = PointIsAirDive(Recast, A) || PointIsAirDive(Recast, B);
					if (bArea && bLink)
					{
						Dive = 1;
						LogDiveSegment(Recast, i, A, B, GotoGoal, bPartial, MeshPts->Num());
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

	/**
	 * Recast PathAirDive means Launch owns the hop. Walk the lip first, then Launch
	 * to the landing — do not refuse because envelope/recipe disagrees (that shake).
	 */
	FVector SteerWaypoint(FCLAgentGotoDriver& D, ACLPlayerCharacter* Char)
	{
		D.SteerReason = FName(TEXT("goal"));
		D.DistLip = -1.f;
		D.bLaunchOk = false;
		D.DistXYToWp = -1.f;
		D.DeltaZToWp = 0.f;
		if (!D.Path.IsValidIndex(D.Index))
		{
			return D.Goal;
		}
		const FVector To = D.Path[D.Index];
		D.DistWp = FVector::Dist2D(Char ? Char->GetActorLocation() : To, To);
		if (!D.PathAirDive.IsValidIndex(D.Index) || D.PathAirDive[D.Index] == 0 || D.Index <= 0 || !Char)
		{
			D.SteerReason = FName(TEXT("wp"));
			return To;
		}
		const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
		const FVector Loc = Char->GetActorLocation();
		const FVector Lip = D.Path[D.Index - 1];
		D.DistLip = FVector::Dist2D(Loc, Lip);
		D.DistXYToWp = FVector::Dist2D(Loc, To);
		D.DeltaZToWp = To.Z - Loc.Z;
		// Diag only — ownership is Recast, not the envelope gate.
		D.bLaunchOk = Move && CLNavAbility::LaunchInEnvelope(Move->GetTune(), CLNavTune::Get(), Loc, To);
		if (D.DistLip > GotoWaypointRadius)
		{
			D.SteerReason = FName(TEXT("diveLip"));
			return Lip;
		}
		D.SteerReason = FName(TEXT("diveLaunch"));
		return To;
	}

	/** DistXY + on-pad settle. True = Tick should return (arrived or cancelled). */
	bool TickArrive(FCLAgentGotoDriver& D, ACLPlayerCharacter* Char, const FVector& Loc)
	{
		if (FVector::Dist2D(Loc, D.Goal) > GotoArriveRadius)
		{
			return false;
		}
		const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
		const bool bNeedLand = D.bFlight || (D.Goal.Z + 80.f < Loc.Z - 120.f);
		const bool bOnPad = CLNavAbility::StandingOnGoalFloor(Loc, D.Goal);
		if (!bNeedLand || (Move && Move->IsMovingOnGround() && !Move->IsDiving() && bOnPad))
		{
			D.Cancel();
			Char->ClearAgentIntent();
			return true;
		}
		return false;
	}

	/** Launch tick. True = this tick was in flight (fail, land, or still airborne). */
	bool TickFlight(FCLAgentGotoDriver& D, ACLPlayerCharacter* Char, float DeltaSeconds, const FVector& Loc)
	{
		if (!D.bFlight)
		{
			return false;
		}
		D.Flight.Tick(DeltaSeconds, Char);
		if (D.Flight.bFailed)
		{
			UE_LOG(LogCallingGoto, Display,
				TEXT("goto flightFail idx=%d loc=(%.0f,%.0f,%.0f) goal=(%.0f,%.0f,%.0f)"),
				D.Index, Loc.X, Loc.Y, Loc.Z, D.Flight.Goal.X, D.Flight.Goal.Y, D.Flight.Goal.Z);
			D.bFlight = false;
			D.Flight.Reset();
			if (D.PathAirDive.IsValidIndex(D.Index))
			{
				D.PathAirDive[D.Index] = 0;
			}
			return true;
		}
		if (D.Flight.bFinished)
		{
			// Intermediate Recast AirDive — keep walking the polyline (do not Cancel goto).
			UE_LOG(LogCallingGoto, Display,
				TEXT("goto flightLand idx=%d loc=(%.0f,%.0f,%.0f) land=(%.0f,%.0f,%.0f)"),
				D.Index, Loc.X, Loc.Y, Loc.Z, D.Flight.Goal.X, D.Flight.Goal.Y, D.Flight.Goal.Z);
			D.bFlight = false;
			D.Flight.Reset();
			if (D.Path.IsValidIndex(D.Index))
			{
				++D.Index;
			}
			D.StuckSeconds = 0.f;
			D.LastWpDist = -1.f;
			return true;
		}
		return true;
	}

	void TickWalk(FCLAgentGotoDriver& D, float DeltaSeconds, UWorld* World, ACLPlayerCharacter* Char, const FVector& Loc)
	{
		while (D.Path.IsValidIndex(D.Index) && FVector::Dist2D(Loc, D.Path[D.Index]) <= GotoWaypointRadius)
		{
			// Recast AirDive: Launch owns — do not walk-advance past the hop.
			if (D.PathAirDive.IsValidIndex(D.Index) && D.PathAirDive[D.Index] != 0)
			{
				break;
			}
			++D.Index;
			D.StuckSeconds = 0.f;
			D.LastWpDist = -1.f;
		}
		if (!D.Path.IsValidIndex(D.Index))
		{
			if (D.RepathLeft > 0)
			{
				--D.RepathLeft;
				FString Error;
				if (D.Start(World, Char, D.Goal, Error, true) && D.Path.IsValidIndex(D.Index))
				{
					return;
				}
			}
		}

		const FVector Steer = SteerWaypoint(D, Char);
		D.SteerAt = Steer;
		const FVector ToTarget = (D.SteerAt - Loc).GetSafeNormal2D();
		if (ToTarget.IsNearlyZero())
		{
			return;
		}

		const float WpDist = FVector::Dist2D(Loc, D.SteerAt);
		if (D.LastWpDist < 0.f || WpDist < D.LastWpDist - 15.f)
		{
			D.StuckSeconds = 0.f;
			D.LastWpDist = WpDist;
		}
		else
		{
			D.StuckSeconds += DeltaSeconds;
			D.LastWpDist = FMath::Min(D.LastWpDist, WpDist);
		}
		if (D.StuckSeconds > 2.5f && D.RepathLeft > 0)
		{
			--D.RepathLeft;
			D.StuckSeconds = 0.f;
			D.LastWpDist = -1.f;
			FString Error;
			if (D.Start(World, Char, D.Goal, Error, true) && D.Path.IsValidIndex(D.Index))
			{
				return;
			}
		}

		// CombatMovement interprets Move as control-yaw local (Y=forward, X=right). Wish toward
		// the Recast waypoint in that basis so look slew does not walk us in circles.
		FVector2D MoveXY = FVector2D::ZeroVector;
		{
			const FRotator Yaw(0.f, Char->GetControlRotation().Yaw, 0.f);
			const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
			MoveXY.X = FVector::DotProduct(ToTarget, Right);
			MoveXY.Y = FVector::DotProduct(ToTarget, Forward);
			const float Mag = MoveXY.Size();
			if (Mag > KINDA_SMALL_NUMBER)
			{
				MoveXY /= Mag;
			}
		}

		bool bJump = false;
		D.bMoveBlocked = false;
		D.FwdKind = NAME_None;
		D.FwdDist = -1.f;
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
			D.FwdKind = FName(CLAgentNavProbe::FwdKindName(Fwd.Kind));
			D.FwdDist = Fwd.Dist;
			// Recast polyline already stays on mesh / DropDown / AirDive links. The 90 cm void
			// probe false-stops on spawn-pad corners and terrace lips → look-only spin.
			if (!D.bNavPath && Drop >= Probe.FloorProbeMaxCm && !bHasLanding)
			{
				const bool bWalkOffLip = Drop > Probe.LipDropMinCm && Drop < 600.f;
				if (!bWalkOffLip)
				{
					D.bMoveBlocked = true;
				}
			}
			if (Fwd.Kind == ECLFwdKind::Wall && Fwd.Dist < 80.f)
			{
				D.bMoveBlocked = true;
			}
			if (D.bMoveBlocked)
			{
				MoveXY = FVector2D::ZeroVector;
			}
			if (D.JumpCooldown <= 0.f && (bJumpCover || bJumpDown || bJumpUp)
				&& (bOnGround || (bJumpUp && JumpsLeft > 0)))
			{
				bJump = true;
				D.JumpCooldown = 0.45f;
				D.StuckSeconds = 0.f;
			}
		}

		D.LastMoveXY = MoveXY;

		{
			static float GotoDiagAcc = 0.f;
			GotoDiagAcc += DeltaSeconds;
			if (GotoDiagAcc >= 0.2f)
			{
				GotoDiagAcc = 0.f;
				const FVector Wp = D.Path.IsValidIndex(D.Index) ? D.Path[D.Index] : D.Goal;
				UE_LOG(LogCallingGoto, Display,
					TEXT("goto tick loc=(%.0f,%.0f,%.0f) idx=%d/%d dive=%d reason=%s launch=%d distLip=%.0f distXY=%.0f dZ=%.0f wp=(%.0f,%.0f,%.0f) steer=(%.0f,%.0f,%.0f) lookYaw=%.1f ctrlYaw=%.1f move=(%.2f,%.2f) blocked=%d fwd=%s/%.0f stuck=%.1f"),
					Loc.X, Loc.Y, Loc.Z, D.Index, D.Path.Num(),
					D.PathAirDive.IsValidIndex(D.Index) ? D.PathAirDive[D.Index] : -1,
					*D.SteerReason.ToString(), D.bLaunchOk ? 1 : 0, D.DistLip, D.DistXYToWp, D.DeltaZToWp,
					Wp.X, Wp.Y, Wp.Z, D.SteerAt.X, D.SteerAt.Y, D.SteerAt.Z,
					ToTarget.Rotation().Yaw, Char->GetControlRotation().Yaw,
					D.LastMoveXY.X, D.LastMoveXY.Y, D.bMoveBlocked ? 1 : 0,
					*D.FwdKind.ToString(), D.FwdDist, D.StuckSeconds);
			}
		}

		// Face the waypoint while moving. Do not slew look while planted — that is the spawn spin.
		if (!MoveXY.IsNearlyZero())
		{
			Char->SetLookGoalYawPitch(true, ToTarget.Rotation().Yaw, true, HeadshotPitch);
		}

		FCLAgentIntent Intent;
		Intent.Move = MoveXY;
		Intent.bSprint = true;
		Intent.bJump = bJump;
		Char->ApplyAgentIntent(Intent);
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
		if (D.Index > 0)
		{
			const float DistLip = FVector::Dist2D(From, D.Path[D.Index - 1]);
			if (DistLip > GotoWaypointRadius)
			{
				return;
			}
		}
		FString LaunchWhy;
		if (const UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			CLNavAbility::ExplainLaunchInEnvelope(Move->GetTune(), CLNavTune::Get(), From, To, LaunchWhy);
		}
		else
		{
			LaunchWhy = TEXT("noMove");
		}
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto ARM_LAUNCH idx=%d | TO=(%.0f,%.0f,%.0f) FROM pawn=(%.0f,%.0f,%.0f) | gotoGoal=(%.0f,%.0f,%.0f) land→goalXY=%.0f | envelope=%s"),
			D.Index, To.X, To.Y, To.Z, From.X, From.Y, From.Z,
			D.Goal.X, D.Goal.Y, D.Goal.Z, FVector::Dist2D(To, D.Goal), *LaunchWhy);
		D.bFlight = true;
		D.Flight.Mode = ECLNavAbilityExecMode::Launch;
		D.Flight.Goal = To;
		D.Flight.LandRadius = 150.f;
		CLBotBookTrace::GotoArm(TEXT("recastAirDive"), From, To, D.bPartial, D.Path.Num());
		D.Flight.Start(Char);
	}

}

void FCLAgentGotoDriver::Cancel()
{
	bActive = false;
	bPartial = false;
	bFlight = false;
	bNavPath = false;
	Path.Reset();
	PathAirDive.Reset();
	Index = 0;
	Goal = FVector::ZeroVector;
	JumpCooldown = 0.f;
	StuckSeconds = 0.f;
	LastWpDist = -1.f;
	Flight.Reset();
	SteerReason = NAME_None;
	SteerAt = FVector::ZeroVector;
	DistLip = -1.f;
	DistWp = -1.f;
	DistXYToWp = -1.f;
	DeltaZToWp = 0.f;
	bLaunchOk = false;
	bMoveBlocked = false;
	LastMoveXY = FVector2D::ZeroVector;
	FwdKind = NAME_None;
	FwdDist = -1.f;
}

bool FCLAgentGotoDriver::Start(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError, bool bFromRepath)
{
	if (!Char || !World)
	{
		OutError = TEXT("no_local_pawn");
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FVector From = Char->GetActorLocation();

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
		FillRecastPath(NavPath, Recast, RecastPts, RecastDive, GoalLoc, NavPath->IsPartial());
	}
	// Recast only — no jump-to / Launch arm. Partial polylines walk + repath (A* node budget).
	const bool bRecastUsable = RecastPts.Num() >= 2;
	const bool bRecastComplete = bRecastUsable && NavPath && !NavPath->IsPartial()
		&& CLNavPathUtil::PathReachesDest(RecastPts, Dest);
	if (!bRecastUsable)
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
	Path = RecastPts;
	PathAirDive = RecastDive;
	Goal = GoalLoc;
	bPartial = !bRecastComplete;
	bNavPath = true;
	Index = 1;
	bActive = true;

	if (Path.IsValidIndex(Index))
	{
		const FVector Steer = SteerWaypoint(*this, Char);
		const FVector ToFirst = (Steer - Char->GetActorLocation()).GetSafeNormal2D();
		if (!ToFirst.IsNearlyZero())
		{
			Char->SetLookGoalYawPitch(true, ToFirst.Rotation().Yaw, true, HeadshotPitch);
		}
	}

	CLBotBookTrace::GotoArm(TEXT("recast"), From, Dest, bPartial, Path.Num());
	{
		const int32 N = FMath::Min(Path.Num(), 6);
		FString Pts;
		for (int32 i = 0; i < N; ++i)
		{
			Pts += FString::Printf(TEXT(" [%d](%.0f,%.0f,%.0f)dive=%d"),
				i, Path[i].X, Path[i].Y, Path[i].Z,
				PathAirDive.IsValidIndex(i) ? PathAirDive[i] : -1);
		}
		UE_LOG(LogCallingGoto, Display,
			TEXT("goto start pts=%d partial=%d idx=%d repath=%d%s"),
			Path.Num(), bPartial ? 1 : 0, Index, RepathLeft, *Pts);
	}
	BeginFlightIfNeeded(*this, Char);
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
	if (TickArrive(*this, Char, Loc))
	{
		return;
	}
	BeginFlightIfNeeded(*this, Char);
	if (TickFlight(*this, Char, DeltaSeconds, Loc))
	{
		return;
	}
	TickWalk(*this, DeltaSeconds, World, Char, Loc);
}
