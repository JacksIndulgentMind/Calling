#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/CLAbilityTypes.h"
#include "CLAbility.generated.h"

class APawn;
class FJsonObject;

/**
 * Data-driven ability instance. Slot bases enforce role; concrete types do the verb.
 * Cooldown lives on the instance. Jump configures movement at loadout time.
 */
UCLASS(Abstract)
class CALLING_API UCLAbility : public UObject
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds);
	virtual bool CanActivate(APawn* Owner) const;
	virtual bool Activate(APawn* Owner);
	virtual void ApplyToMovement(APawn* Owner);
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale);
	void BeginCooldown();

	UFUNCTION(BlueprintPure, Category = "Calling|Ability")
	FName GetId() const { return Id; }

	UFUNCTION(BlueprintPure, Category = "Calling|Ability")
	ECLAbilitySlot GetSlot() const { return Slot; }

	UFUNCTION(BlueprintPure, Category = "Calling|Ability")
	float GetCooldownRemaining() const { return RemainingCooldown; }

	UFUNCTION(BlueprintPure, Category = "Calling|Ability")
	float GetCooldown() const { return Cooldown; }

	UFUNCTION(BlueprintPure, Category = "Calling|Ability")
	FString GetDisplayName() const { return DisplayName; }

	UPROPERTY()
	FName Id = NAME_None;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	ECLAbilitySlot Slot = ECLAbilitySlot::Grenade;

	UPROPERTY()
	float Cooldown = 8.f;

	UPROPERTY()
	float RefCooldown = 0.f;

	UPROPERTY()
	float Duration = 0.f;

	UPROPERTY()
	float Damage = 0.f;

	UPROPERTY()
	float Range = 0.f;

	UPROPERTY()
	float Radius = 0.f;

protected:
	float RemainingCooldown = 0.f;
	float ActiveSecondsRemaining = 0.f;
	TWeakObjectPtr<APawn> ActiveOwner;

	static float JsonNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Fallback);
	static bool JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Fallback);
	static int32 JsonInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Fallback);
	static FString JsonString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Fallback);
};
