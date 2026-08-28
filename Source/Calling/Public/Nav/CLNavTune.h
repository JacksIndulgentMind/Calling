#pragma once

#include "CoreMinimal.h"

struct FCLNavProbeTune
{
	float MaxCm = 1800.f;
	float JumpableHeadClearCm = 500.f;
	float JumpFaceCm = 180.f;
	float HeadLiftCm = 70.f;
	float WalkableNormalZ = 0.55f;
	float UpSlopeMin = 0.04f;
	float LipDropMinCm = 30.f;
	float FloorProbeMaxCm = 2500.f;
	float SampleStepCm = 70.f;
	float WalkOffGapMaxCm = 250.f;
	float CoverDepthCm = 160.f;
	float StartBackupCm = 40.f;
};

struct FCLNavLinkTune
{
	FName Name;
	float JumpLength = 200.f;
	float JumpDistanceFromEdge = 20.f;
	FString JumpMaxDepth;
	FString JumpHeight;
	FString JumpEndsHeightTolerance;
	float SamplingSeparationFactor = 2.f;
	float FilterDistanceThreshold = 120.f;
	FString DownArea = TEXT("default");
	FString UpArea = TEXT("default");
};

struct FCLNavTune
{
	float JumpApexCm = 400.f;
	float CoverHeightCm = 140.f;
	float MaxStepHeightCm = 70.f;
	float AgentRadiusCm = 42.f;
	float AgentHeightCm = 192.f;
	float AgentMaxSlopeDeg = 55.f;
	/** 0 = uncapped MaxLaunchXY. Narrows AirDive bake+runtime search; must be <= MaxLaunchXY. */
	float AirDiveSearchMaxCm = 0.f;
	FCLNavProbeTune Probe;
	TArray<FCLNavLinkTune> Links;
};

namespace CLNavTune
{
	const FCLNavTune& Get();
	float ResolveScalar(const FString& TokenOrNumber, float Fallback, const FCLNavTune& Tune, float SurvivingDropCm);
	bool IsAirDiveLink(FName Name);
}
