#include "Nav/CLNavLinkPolicy.h"
#include "Nav/CLNavTune.h"
#include "Nav/CLNavArea_LongJump.h"
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
		return UNavArea_Default::StaticClass();
	}
}

void CLNavLinkPolicy::ApplyToRecast(ARecastNavMesh& Recast, float SurvivingDropCm)
{
	const FCLNavTune& Tune = CLNavTune::Get();
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

	for (int32 i = 0; i < Tune.Links.Num(); ++i)
	{
		const FCLNavLinkTune& Src = Tune.Links[i];
		FNavLinkGenerationJumpConfig& Dst = JumpConfigs[i];
		Dst.bEnabled = true;
		Dst.Name = Src.Name;
		Dst.JumpLength = Src.JumpLength;
		Dst.JumpDistanceFromEdge = Src.JumpDistanceFromEdge;
		Dst.JumpMaxDepth = CLNavTune::ResolveScalar(Src.JumpMaxDepth, 8.f, Tune, SurvivingDropCm);
		Dst.JumpHeight = CLNavTune::ResolveScalar(Src.JumpHeight, Tune.CoverHeightCm, Tune, SurvivingDropCm);
		Dst.JumpEndsHeightTolerance = CLNavTune::ResolveScalar(Src.JumpEndsHeightTolerance, 12.f, Tune, SurvivingDropCm);
		Dst.SamplingSeparationFactor = Src.SamplingSeparationFactor;
		Dst.FilterDistanceThreshold = Src.FilterDistanceThreshold;
		Dst.LinkBuilderFlags = LinkPts;
		Dst.DownDirectionAreaClass = AreaFromName(Src.DownArea);
		Dst.UpDirectionAreaClass = AreaFromName(Src.UpArea);
	}
	for (int32 i = Tune.Links.Num(); i < JumpConfigs.Num(); ++i)
	{
		JumpConfigs[i].bEnabled = false;
	}
}
