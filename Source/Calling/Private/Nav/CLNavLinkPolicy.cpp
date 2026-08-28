#include "Nav/CLNavLinkPolicy.h"
#include "Nav/CLNavTune.h"
#include "Nav/CLNavArea_LongJump.h"
#include "Nav/CLNavArea_AirDive.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Nav/CLNavAbilityValidate.h"
#include "Core/CLTunes.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/LinkGenerationConfig.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"

namespace
{
	TSubclassOf<UNavArea> AreaFromName(const FString& Name)
	{
		if (Name.Equals(TEXT("null"), ESearchCase::IgnoreCase))
		{
			return UNavArea_Null::StaticClass();
		}
		if (Name.Equals(TEXT("longJump"), ESearchCase::IgnoreCase))
		{
			return UCLNavArea_LongJump::StaticClass();
		}
		if (Name.Equals(TEXT("airDive"), ESearchCase::IgnoreCase))
		{
			return UCLNavArea_AirDive::StaticClass();
		}
		return UNavArea_Default::StaticClass();
	}
}

void CLNavLinkPolicy::ApplyToRecast(ARecastNavMesh& Recast, float SurvivingDropCm)
{
	const FCLNavTune& Tune = CLNavTune::Get();
	FCLMovementTune Move;
	Move.LoadFromIni();
	const FCLNavAbilityValidateResult Valid = CLNavAbilityValidate::Check(Move, Tune, SurvivingDropCm);
	CLNavAbilityValidate::Report(&Recast, Valid);

	Recast.AgentRadius = Tune.AgentRadiusCm;
	Recast.AgentHeight = Tune.AgentHeightCm;
	Recast.AgentMaxSlope = Tune.AgentMaxSlopeDeg;
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::Default, Tune.MaxStepHeightCm);
	Recast.bGenerateNavLinks = true;

	TArray<FNavLinkGenerationJumpConfig>& JumpConfigs =
		const_cast<TArray<FNavLinkGenerationJumpConfig>&>(Recast.GetNavLinkJumpConfigs());
	const int32 Wanted = FMath::Max(Tune.Links.Num(), 1);
	if (JumpConfigs.Num() < Wanted)
	{
		JumpConfigs.SetNum(Wanted);
	}

	const uint16 LinkPts = static_cast<uint16>(ENavLinkBuilderFlags::CreateCenterPointLink)
		| static_cast<uint16>(ENavLinkBuilderFlags::CreateExtremityLink);

	// XY voxel size (Epic default 25; guide range ~32). Not TileSize — tiles are dirty chunks.
	const float CellSize = 32.f;
	Recast.SetCellSize(ENavigationDataResolution::Low, CellSize);
	Recast.SetCellSize(ENavigationDataResolution::Default, CellSize);
	Recast.SetCellSize(ENavigationDataResolution::High, CellSize);
	// TileSizeUU is cache/rebuild granularity. Epic clamp: max(16*CellSize, 4*AgentRadius) .. 1024*CellSize.
	// Never derive this from JumpLength or the island chord.
	const float MinTile = FMath::Max3(300.f, CellSize * 16.f, Recast.AgentRadius * 4.f);
	const float MaxTile = CellSize * 1024.f;
	Recast.TileSizeUU = FMath::Clamp(CellSize * 32.f, MinTile, MaxTile);
	Recast.bFixedTilePoolSize = false;
	Recast.TilePoolSize = FMath::Max(Recast.TilePoolSize, 4096);
	Recast.AverageLayersPerTile = FMath::Max(Recast.AverageLayersPerTile, 8.f);
	Recast.ExpectedMaxLayersPerTile = FMath::Max(Recast.ExpectedMaxLayersPerTile, 12);
	Recast.bMinimizeLinkPoolSize = false;
	Recast.bAllowNavLinkAsPathEnd = true;
	Recast.DefaultMaxSearchNodes = FMath::Max(Recast.DefaultMaxSearchNodes, 4096.f);
	Recast.DefaultMaxHierarchicalSearchNodes = FMath::Max(Recast.DefaultMaxHierarchicalSearchNodes, 4096.f);
	Recast.SimplificationElevationRatio = FMath::Max(Recast.SimplificationElevationRatio, 1.f);
	Recast.bPerformVoxelFiltering = true;
	Recast.bSortNavigationAreasByCost = true;
	// Court Z≈−20 m plus the island in the same nav bounds overflows Recast’s
	// 255-span height if CellHeight stays 10. That is Z voxels, not CellSize.
	// Locked at 30 (255×30 = 7650 cm). Do not iterate voxels to pass AABB.
	const float CellH = 30.f;
	Recast.SetCellHeight(ENavigationDataResolution::Low, CellH);
	Recast.SetCellHeight(ENavigationDataResolution::Default, CellH);
	Recast.SetCellHeight(ENavigationDataResolution::High, CellH);
	// Coarse CellHeight flattens the 9° ramp into ~80 cm ledges. Recast climb
	// must clear those voxels; pawn MaxStepHeight stays Tune (70).
	const float RecastStep = FMath::Max(Tune.MaxStepHeightCm, CellH * 4.f);
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::Low, RecastStep);
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::Default, RecastStep);
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::High, RecastStep);

	// Triple-jump peak above the walkable edge (AirDive leaves the lip by jumping).
	const float TripleApex = CLNavAbility::JumpApexUpCm(Move, FMath::Max(1, Move.MaxJumps));

	for (int32 i = 0; i < Tune.Links.Num(); ++i)
	{
		const FCLNavLinkTune& Src = Tune.Links[i];
		FNavLinkGenerationJumpConfig& Dst = JumpConfigs[i];
		const bool bAirDive = CLNavTune::IsAirDiveLink(Src.Name);
		Dst.bEnabled = !(bAirDive && !Valid.bApplyAirDiveLink);
		Dst.Name = Src.Name;
		// Pass NavTune jump knobs through. No Calling ceilings (not MaxLaunchXY, not
		// place chord, not Epic UIMax-as-hard-cap). Epic UIMax is a slider only.
		Dst.JumpLength = Src.JumpLength;
		Dst.JumpDistanceFromEdge = Src.JumpDistanceFromEdge;
		Dst.JumpMaxDepth = CLNavTune::ResolveScalar(Src.JumpMaxDepth, 8.f, Tune, SurvivingDropCm);
		Dst.JumpHeight = CLNavTune::ResolveScalar(
			Src.JumpHeight,
			bAirDive ? TripleApex : Tune.CoverHeightCm,
			Tune,
			SurvivingDropCm);
		Dst.JumpEndsHeightTolerance = CLNavTune::ResolveScalar(
			Src.JumpEndsHeightTolerance, 12.f, Tune, SurvivingDropCm);
		// Epic ClampMin=1 on SamplingSeparationFactor — floor only, not a Calling cap.
		Dst.SamplingSeparationFactor = FMath::Max(1.f, Src.SamplingSeparationFactor);
		Dst.FilterDistanceThreshold = Src.FilterDistanceThreshold;
		Dst.LinkBuilderFlags = LinkPts;
		Dst.DownDirectionAreaClass = AreaFromName(Src.DownArea);
		Dst.UpDirectionAreaClass = AreaFromName(Src.UpArea);
	}
	for (int32 i = Tune.Links.Num(); i < JumpConfigs.Num(); ++i)
	{
		JumpConfigs[i].bEnabled = false;
	}
	Recast.OnNavAreaAdded(UCLNavArea_AirDive::StaticClass(), 0);
	Recast.OnNavAreaAdded(UCLNavArea_LongJump::StaticClass(), 0);
	Recast.RecreateDefaultFilter();
	Recast.ConditionalConstructGenerator();
}
