#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/DLItemInstance.h"
#include "DLWeaponBehaviorComponent.generated.h"

/**
 * Applies up to 4 modifier behavior hooks with capped scale so muscle memory holds.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLWeaponBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLWeaponBehaviorComponent();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void SetModifiers(const TArray<FDLModifierRoll>& InModifiers);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void NotifyFired(bool bFirstShotAfterReady);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void NotifyKill();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Weapon")
	void NotifyPrecisionKill();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetAccuracyMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetDamageMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetReloadSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Weapon")
	float GetSlideAdsAccuracyBonus() const;

protected:
	UPROPERTY()
	TArray<FDLModifierRoll> Modifiers;

	float MaxBehaviorScale = 0.15f;
	float OpeningShotTimer = 0.f;
	float KillClipTimer = 0.f;
	float OutlawTimer = 0.f;
	bool bPendingFirstShot = true;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	float FindBehaviorScale(FName BehaviorId) const;
};
