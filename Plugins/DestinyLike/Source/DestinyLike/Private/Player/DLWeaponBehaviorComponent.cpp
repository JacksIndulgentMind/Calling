#include "Player/DLWeaponBehaviorComponent.h"
#include "Misc/ConfigCacheIni.h"

UDLWeaponBehaviorComponent::UDLWeaponBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDLWeaponBehaviorComponent::SetModifiers(const TArray<FDLModifierRoll>& InModifiers)
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxModifierBehaviorScale"), MaxBehaviorScale, GGameIni);
	Modifiers = InModifiers;
	for (FDLModifierRoll& Mod : Modifiers)
	{
		Mod.BehaviorScale = FMath::Clamp(Mod.BehaviorScale, 0.f, MaxBehaviorScale);
	}
}

float UDLWeaponBehaviorComponent::FindBehaviorScale(FName BehaviorId) const
{
	for (const FDLModifierRoll& Mod : Modifiers)
	{
		if (Mod.BehaviorId == BehaviorId)
		{
			return Mod.BehaviorScale;
		}
	}
	return 0.f;
}

void UDLWeaponBehaviorComponent::NotifyFired(bool bFirstShotAfterReady)
{
	if (bFirstShotAfterReady || bPendingFirstShot)
	{
		OpeningShotTimer = 0.6f;
		bPendingFirstShot = false;
	}
}

void UDLWeaponBehaviorComponent::NotifyKill()
{
	if (FindBehaviorScale(FName(TEXT("kill_damage_window"))) > 0.f)
	{
		KillClipTimer = 1.5f;
	}
}

void UDLWeaponBehaviorComponent::NotifyPrecisionKill()
{
	if (FindBehaviorScale(FName(TEXT("precision_reload_speed"))) > 0.f)
	{
		OutlawTimer = 2.0f;
	}
	NotifyKill();
}

float UDLWeaponBehaviorComponent::GetAccuracyMultiplier() const
{
	float Mul = 1.f;
	if (OpeningShotTimer > 0.f)
	{
		Mul += FindBehaviorScale(FName(TEXT("opening_shot_range")));
		Mul += FindBehaviorScale(FName(TEXT("first_shot_accuracy")));
	}
	return Mul;
}

float UDLWeaponBehaviorComponent::GetDamageMultiplier() const
{
	float Mul = 1.f;
	if (KillClipTimer > 0.f)
	{
		Mul += FindBehaviorScale(FName(TEXT("kill_damage_window")));
	}
	return Mul;
}

float UDLWeaponBehaviorComponent::GetReloadSpeedMultiplier() const
{
	float Mul = 1.f;
	if (OutlawTimer > 0.f)
	{
		Mul += FindBehaviorScale(FName(TEXT("precision_reload_speed")));
	}
	return Mul;
}

float UDLWeaponBehaviorComponent::GetSlideAdsAccuracyBonus() const
{
	return FindBehaviorScale(FName(TEXT("ads_while_slide_bonus")));
}

void UDLWeaponBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	OpeningShotTimer = FMath::Max(0.f, OpeningShotTimer - DeltaTime);
	KillClipTimer = FMath::Max(0.f, KillClipTimer - DeltaTime);
	OutlawTimer = FMath::Max(0.f, OutlawTimer - DeltaTime);
}
