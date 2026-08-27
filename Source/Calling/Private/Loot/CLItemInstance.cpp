#include "Loot/CLItemInstance.h"

namespace
{
	float ClampDelta(float Value, float MaxAbs)
	{
		return FMath::Clamp(Value, -MaxAbs, MaxAbs);
	}

	void ApplyDelta(FCLWeaponStats& Out, const FCLWeaponStats& Delta, float MaxStatDelta)
	{
		Out.Impact += ClampDelta(Delta.Impact, MaxStatDelta * 100.f);
		Out.Range = FMath::Clamp(Out.Range + ClampDelta(Delta.Range, MaxStatDelta), 0.f, 1.5f);
		Out.Stability = FMath::Clamp(Out.Stability + ClampDelta(Delta.Stability, MaxStatDelta), 0.f, 1.5f);
		Out.Handling = FMath::Clamp(Out.Handling + ClampDelta(Delta.Handling, MaxStatDelta), 0.f, 1.5f);
		Out.Reload = FMath::Clamp(Out.Reload + ClampDelta(Delta.Reload, MaxStatDelta), 0.f, 1.5f);
		Out.FlinchResist = FMath::Clamp(Out.FlinchResist + ClampDelta(Delta.FlinchResist, MaxStatDelta), 0.f, 1.f);
		Out.AdsSpeed = FMath::Clamp(Out.AdsSpeed + ClampDelta(Delta.AdsSpeed, MaxStatDelta), 0.f, 1.f);
		Out.MobilityBonus = FMath::Clamp(Out.MobilityBonus + ClampDelta(Delta.MobilityBonus, MaxStatDelta), 0.f, 0.25f);
		Out.Grip = FMath::Clamp(Out.Grip + ClampDelta(Delta.Grip, MaxStatDelta), 0.05f, 1.f);
		Out.Compensator = FMath::Clamp(Out.Compensator + ClampDelta(Delta.Compensator, MaxStatDelta), 0.f, 1.f);
		Out.DrawSeconds = FMath::Max(0.05f, Out.DrawSeconds + ClampDelta(Delta.DrawSeconds, MaxStatDelta));
		Out.StowSeconds = FMath::Max(0.05f, Out.StowSeconds + ClampDelta(Delta.StowSeconds, MaxStatDelta));
		Out.MassKg = FMath::Max(0.2f, Out.MassKg + ClampDelta(Delta.MassKg, MaxStatDelta * 2.f));
	}
}

void FCLItemInstance::RecomputeFinalStats(float MaxStatDelta)
{
	FinalStats = BaseStats;
	for (const FCLModifierRoll& Mod : Modifiers)
	{
		ApplyDelta(FinalStats, Mod.StatDelta, MaxStatDelta);
	}
}
