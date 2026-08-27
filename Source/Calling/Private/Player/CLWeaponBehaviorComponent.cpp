#include "Player/CLWeaponBehaviorComponent.h"
#include "Player/CLModifierBehavior.h"
#include "Misc/ConfigCacheIni.h"

UCLWeaponBehaviorComponent::UCLWeaponBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCLWeaponBehaviorComponent::SetModifiers(const TArray<FCLModifierRoll>& InModifiers)
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxModifierBehaviorScale"), MaxBehaviorScale, GGameIni);
	Modifiers = InModifiers;
	BoundBehaviors.Reset();
	for (FCLModifierRoll& Mod : Modifiers)
	{
		Mod.BehaviorScale = FMath::Clamp(Mod.BehaviorScale, 0.f, MaxBehaviorScale);
		if (TSharedPtr<ICLModifierBehavior> Bound = CLMakeModifierBehavior(Mod.BehaviorId, Mod.BehaviorScale))
		{
			BoundBehaviors.Add(MoveTemp(Bound));
		}
	}
}

void UCLWeaponBehaviorComponent::StartKillClipWindow(float Seconds)
{
	KillClipTimer = Seconds;
}

void UCLWeaponBehaviorComponent::StartOutlawWindow(float Seconds)
{
	OutlawTimer = Seconds;
}

bool UCLWeaponBehaviorComponent::HasProxDetonate() const
{
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound && Bound->HasProxDetonate())
		{
			return true;
		}
	}
	return false;
}

void UCLWeaponBehaviorComponent::NotifyFired(bool bFirstShotAfterReady)
{
	if (bFirstShotAfterReady || bPendingFirstShot)
	{
		OpeningShotTimer = 0.6f;
		bPendingFirstShot = false;
	}
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Bound->OnFired(*this, bFirstShotAfterReady);
		}
	}
}

void UCLWeaponBehaviorComponent::NotifyKill()
{
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Bound->OnKill(*this);
		}
	}
}

void UCLWeaponBehaviorComponent::NotifyPrecisionKill()
{
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Bound->OnPrecisionKill(*this);
		}
	}
	NotifyKill();
}

float UCLWeaponBehaviorComponent::GetAccuracyMultiplier() const
{
	float Mul = 1.f;
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Mul += Bound->AccuracyBonus(*this);
		}
	}
	return Mul;
}

float UCLWeaponBehaviorComponent::GetDamageMultiplier() const
{
	float Mul = 1.f;
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Mul += Bound->DamageBonus(*this);
		}
	}
	return Mul;
}

float UCLWeaponBehaviorComponent::GetReloadSpeedMultiplier() const
{
	float Mul = 1.f;
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Mul += Bound->ReloadSpeedBonus(*this);
		}
	}
	return Mul;
}

float UCLWeaponBehaviorComponent::GetSlideAdsAccuracyBonus() const
{
	float Bonus = 0.f;
	for (const TSharedPtr<ICLModifierBehavior>& Bound : BoundBehaviors)
	{
		if (Bound)
		{
			Bonus += Bound->SlideAdsAccuracyBonus(*this);
		}
	}
	return Bonus;
}

void UCLWeaponBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	OpeningShotTimer = FMath::Max(0.f, OpeningShotTimer - DeltaTime);
	KillClipTimer = FMath::Max(0.f, KillClipTimer - DeltaTime);
	OutlawTimer = FMath::Max(0.f, OutlawTimer - DeltaTime);
}
