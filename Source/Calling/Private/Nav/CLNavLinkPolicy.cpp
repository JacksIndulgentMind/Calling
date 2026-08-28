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

	const float AirDiveLen = CLNavAbility::SearchRadiusCm(Move, Tune, CLNavAbility::AirDiveRefDropCm());
	// Off-mesh far ends connect only to the same tile or an edge neighbor.
	// Island DistXY is ~31 m; 10 m tiles leave the pad two hops away (validEnds=Left).
	// Stay under the old hull-sized ~40 m tiles that collapsed spawn→court ramps.
	const float IslandSpan = CLNavAbility::AirDiveBakeJumpLengthCm(Move, Tune, 40.f);
	Recast.TileSizeUU = FMath::Clamp(IslandSpan + 500.f, 2000.f, 3600.f);
	// Do not inflate TileSizeUU to JumpLength. UE 5.8 Recast allows JumpLength >
	// tile size via LinkSpillDistance. Hull-sized tiles (~40 m) collapse the
	// 3-lane into ~18 tiles and break spawn→court walking.
	Recast.bFixedTilePoolSize = false;
	Recast.TilePoolSize = FMath::Max(Recast.TilePoolSize, 256);
	Recast.AverageLayersPerTile = FMath::Max(Recast.AverageLayersPerTile, 8.f);
	Recast.ExpectedMaxLayersPerTile = FMath::Max(Recast.ExpectedMaxLayersPerTile, 12);
	// Court Z≈−20 m plus the AirDive island in the same nav bounds overflows
	// Recast’s 255-span height if CellHeight stays 10. Keep voxel columns under ~240.
	const float AirDiveMaxDepth = CLNavAbility::AirDiveChordMaxDepthCm(Tune.JumpApexCm);
	const float CellH = FMath::Clamp(AirDiveMaxDepth / 100.f, 25.f, 40.f);
	Recast.SetCellHeight(ENavigationDataResolution::Low, CellH);
	Recast.SetCellHeight(ENavigationDataResolution::Default, CellH);
	Recast.SetCellHeight(ENavigationDataResolution::High, CellH);
	// Coarse CellHeight flattens the 9° ramp into ~80 cm ledges. Recast climb
	// must clear those voxels; pawn MaxStepHeight stays Tune (70).
	const float RecastStep = FMath::Max(Tune.MaxStepHeightCm, CellH * 4.f);
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::Low, RecastStep);
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::Default, RecastStep);
	Recast.SetAgentMaxStepHeight(ENavigationDataResolution::High, RecastStep);
	for (int32 i = 0; i < Tune.Links.Num(); ++i)
	{
		const FCLNavLinkTune& Src = Tune.Links[i];
		FNavLinkGenerationJumpConfig& Dst = JumpConfigs[i];
		const bool bAirDive = CLNavTune::IsAirDiveLink(Src.Name);
		const bool bAirDiveDown = Src.Name.ToString().Equals(TEXT("AirDiveDown"), ESearchCase::IgnoreCase);
		Dst.bEnabled = !(bAirDive && !Valid.bApplyAirDiveLink);
		Dst.Name = Src.Name;
		// Recast floors the parabola END only. Full MaxLaunchXY at strain drop overshoots
		// the apex-survivable island into void (and can collide with the pad mid-spine).
		Dst.JumpLength = bAirDiveDown
			? CLNavAbility::AirDiveBakeJumpLengthCm(Move, Tune, Src.JumpDistanceFromEdge)
			: (bAirDive ? AirDiveLen : Src.JumpLength);
		Dst.JumpDistanceFromEdge = Src.JumpDistanceFromEdge;
		Dst.JumpMaxDepth = bAirDive
			? AirDiveMaxDepth
			: CLNavTune::ResolveScalar(Src.JumpMaxDepth, 8.f, Tune, SurvivingDropCm);
		// Recast samples a jump parabola. Air dive is hang+pin, not a hop: height 0
		// so the spine drops JumpMaxDepth over JumpLength and can hit the island.
		Dst.JumpHeight = bAirDive
			? 0.f
			: CLNavTune::ResolveScalar(Src.JumpHeight, Tune.CoverHeightCm, Tune, SurvivingDropCm);
		Dst.JumpEndsHeightTolerance = bAirDive
			? FMath::Max(CLNavAbility::AirDiveChordEndZTolCm,
				AirDiveMaxDepth - CLNavAbility::AirDivePadDropFromLipCm(Tune.JumpApexCm) + 250.f)
			: CLNavTune::ResolveScalar(Src.JumpEndsHeightTolerance, 12.f, Tune, SurvivingDropCm);
		Dst.SamplingSeparationFactor = bAirDive ? 1.f : Src.SamplingSeparationFactor;
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
	Recast.ConditionalConstructGenerator();
}
