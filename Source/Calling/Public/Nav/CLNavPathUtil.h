#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"

namespace CLNavPathUtil
{
	/** Same arrive rule goto uses for Recast-complete play (XY + Z slack). Not the bake FindPath mesh bar. */
	inline constexpr float PathArriveXYCm = 250.f;
	inline constexpr float PathArriveZCm = 400.f;

	inline bool PathReachesDest(const TArray<FVector>& Pts, const FVector& Dest)
	{
		if (Pts.Num() < 2)
		{
			return false;
		}
		const FVector& Last = Pts.Last();
		return FVector::Dist2D(Last, Dest) <= PathArriveXYCm
			&& FMath::Abs(Last.Z - Dest.Z) <= PathArriveZCm;
	}

	/**
	 * Bake FindPath success: path start on from (lip) nav poly, path end on dest (pad) nav poly.
	 * Compares projected NodeRefs to the already-projected from/dest locations.
	 */
	inline bool PathEndsOnFromAndDestNav(
		UNavigationSystemV1& NavSys,
		const TArray<FVector>& PathPts,
		const FNavLocation& FromNav,
		const FNavLocation& DestNav,
		const FVector& Extent)
	{
		if (PathPts.Num() < 2
			|| FromNav.NodeRef == INVALID_NAVNODEREF
			|| DestNav.NodeRef == INVALID_NAVNODEREF)
		{
			return false;
		}
		FNavLocation StartNav;
		FNavLocation EndNav;
		if (!NavSys.ProjectPointToNavigation(PathPts[0], StartNav, Extent)
			|| !NavSys.ProjectPointToNavigation(PathPts.Last(), EndNav, Extent))
		{
			return false;
		}
		return StartNav.NodeRef == FromNav.NodeRef && EndNav.NodeRef == DestNav.NodeRef;
	}
}
