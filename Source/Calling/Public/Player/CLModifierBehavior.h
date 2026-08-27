#pragma once

#include "CoreMinimal.h"

class UCLWeaponBehaviorComponent;

/**
 * Modifier verb bound at SetModifiers. Per-frame queries hit this, not FName scans.
 */
class ICLModifierBehavior
{
public:
	virtual ~ICLModifierBehavior() = default;

	virtual void OnFired(UCLWeaponBehaviorComponent& Owner, bool bFirstShotAfterReady) {}
	virtual void OnKill(UCLWeaponBehaviorComponent& Owner) {}
	virtual void OnPrecisionKill(UCLWeaponBehaviorComponent& Owner) {}
	virtual float AccuracyBonus(const UCLWeaponBehaviorComponent& Owner) const { return 0.f; }
	virtual float DamageBonus(const UCLWeaponBehaviorComponent& Owner) const { return 0.f; }
	virtual float ReloadSpeedBonus(const UCLWeaponBehaviorComponent& Owner) const { return 0.f; }
	virtual float SlideAdsAccuracyBonus(const UCLWeaponBehaviorComponent& Owner) const { return 0.f; }
	virtual bool HasProxDetonate() const { return false; }
};

TSharedPtr<ICLModifierBehavior> CLMakeModifierBehavior(FName BehaviorId, float Scale);
