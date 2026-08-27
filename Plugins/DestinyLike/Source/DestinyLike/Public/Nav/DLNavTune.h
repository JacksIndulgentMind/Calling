#pragma once

#include "CoreMinimal.h"

struct FDLNavProbeTune
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

struct FDLNavLinkTune
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

struct FDLNavTune
{
	float JumpApexCm = 400.f;
	float CoverHeightCm = 140.f;
	float MaxStepHeightCm = 70.f;
	float AgentRadiusCm = 42.f;
	float AgentHeightCm = 192.f;
	float AgentMaxSlopeDeg = 55.f;
	FDLNavProbeTune Probe;
	TArray<FDLNavLinkTune> Links;
};

namespace DLNavTune
{
	const FDLNavTune& Get();
	float ResolveScalar(const FString& TokenOrNumber, float Fallback, const FDLNavTune& Tune, float SurvivingDropCm);
}
