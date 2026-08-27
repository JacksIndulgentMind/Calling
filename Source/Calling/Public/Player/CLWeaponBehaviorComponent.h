#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Loot/CLItemInstance.h"
#include "Player/CLModifierBehavior.h"
#include "CLWeaponBehaviorComponent.generated.h"

/**
 * Applies up to 4 modifier behavior hooks with capped scale so muscle memory holds.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLWeaponBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLWeaponBehaviorComponent();

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void SetModifiers(const TArray<FCLModifierRoll>& InModifiers);

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void NotifyFired(bool bFirstShotAfterReady);

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void NotifyKill();

	UFUNCTION(BlueprintCallable, Category = "Calling|Weapon")
	void NotifyPrecisionKill();

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetAccuracyMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetDamageMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetReloadSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Weapon")
	float GetSlideAdsAccuracyBonus() const;

	bool HasProxDetonate() const;
	bool IsOpeningShotActive() const { return OpeningShotTimer > 0.f; }
	bool IsKillClipActive() const { return KillClipTimer > 0.f; }
	bool IsOutlawActive() const { return OutlawTimer > 0.f; }
	void StartKillClipWindow(float Seconds);
	void StartOutlawWindow(float Seconds);

protected:
	UPROPERTY()
	TArray<FCLModifierRoll> Modifiers;

	TArray<TSharedPtr<ICLModifierBehavior>> BoundBehaviors;

	float MaxBehaviorScale = 0.15f;
	float OpeningShotTimer = 0.f;
	float KillClipTimer = 0.f;
	float OutlawTimer = 0.f;
	bool bPendingFirstShot = true;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
