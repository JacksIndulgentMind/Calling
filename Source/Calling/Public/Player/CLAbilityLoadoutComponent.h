#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CLTypes.h"
#include "Ability/CLAbilityTypes.h"
#include "CLAbilityLoadoutComponent.generated.h"

class UCLAbility;

UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLAbilityLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLAbilityLoadoutComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool LoadFromCharacter(const FCLCharacterAppearance& Character);

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool TryGrenade();

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool TryMelee();

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool TryDash();

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool TryShield();

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool TryEvasion();

	UFUNCTION(BlueprintCallable, Category = "Calling|Ability")
	bool TrySuper();

	UFUNCTION(BlueprintPure, Category = "Calling|Ability")
	ECLClassId GetClassId() const { return ClassId; }

	UCLAbility* GetSlot(ECLAbilitySlot Slot) const;

protected:
	bool TryActivate(ECLAbilitySlot Slot);
	void ClearSlots();

	UPROPERTY()
	ECLClassId ClassId = ECLClassId::Vanguard;

	UPROPERTY()
	TObjectPtr<UCLAbility> Grenade;

	UPROPERTY()
	TObjectPtr<UCLAbility> Shield;

	UPROPERTY()
	TObjectPtr<UCLAbility> Evasion;

	UPROPERTY()
	TObjectPtr<UCLAbility> Dash;

	UPROPERTY()
	TObjectPtr<UCLAbility> Melee;

	UPROPERTY()
	TObjectPtr<UCLAbility> Jump;

	UPROPERTY()
	TObjectPtr<UCLAbility> SuperAbility;
};
