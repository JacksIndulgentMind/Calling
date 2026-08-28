#pragma once

#include "CoreMinimal.h"

class ACLGreyboxFloors;
enum class ECLGreyboxLayout : uint8;

struct FCLPvpThreeLaneRecipe
{
	float PadDepthM = 10.f;
	float CourtM = 48.f;
	float RavineM = 20.f;
	float WidthM = 48.f;
	float SlabZ = 40.f;
	float LaneW = 12.f;
	float PadInnerM = 140.f;
	float LaneYMeters[3] = { -18.f, 0.f, 18.f };
	float CrossFractions[2] = { 0.33f, 0.66f };
	float HalfCoverZ = 85.f;
	float LaneStepM[3] = { 8.f, 11.f, 14.f };
	float LanePhaseM[3] = { 0.f, 3.5f, 7.f };
	float RailStepM = 16.f;
	float CourtRadiiM[3] = { 6.f, 12.f, 18.f };
	int32 RingCount[3] = { 6, 8, 10 };
	float MenhirRadiusM = 9.5f;
	int32 MenhirCount = 8;
	float MenhirDepthCm = 220.f;
	float MenhirWidthCm = 90.f;
	float MenhirHeightCm = 280.f;
	float MenhirSpanCm = 220.f;
	float MenhirPostWidthCm = 80.f;
	float MenhirPostDepthCm = 80.f;
	float MenhirLintelHeightCm = 45.f;

	bool Load();

	/** South court nav edge → canary island. PadDropFromLipCm is (3000 − 300) − JumpApexUpCm(3).
	 *  ChordCm is Recast launch-plane intercept (x0); Ends apply rim inset 200. Recast knobs do not move this pad. */
	void EdgeAirDiveEnds(FVector& OutLip, FVector& OutPad, float ChordCm, float JumpDistanceFromEdgeCm,
		float PadDropFromLipCm) const
	{
		const float M = 100.f;
		const float PitZ = -RavineM * M;
		const float Rim = CourtM * 0.5f * M;
		OutLip = FVector(0.f, -Rim + 200.f, PitZ);
		(void)JumpDistanceFromEdgeCm;
		const float Inward = 200.f;
		OutPad = FVector(0.f, OutLip.Y - ChordCm + Inward, OutLip.Z - PadDropFromLipCm);
	}
};

class ICLGreyboxLayout
{
public:
	virtual ~ICLGreyboxLayout() = default;
	virtual void Build(ACLGreyboxFloors& Floors) = 0;
};

TUniquePtr<ICLGreyboxLayout> CLMakeGreyboxLayout(ECLGreyboxLayout Id);
