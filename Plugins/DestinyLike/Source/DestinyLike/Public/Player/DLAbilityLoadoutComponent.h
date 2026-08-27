#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DLTypes.h"
#include "Ability/DLAbilityTypes.h"
#include "DLAbilityLoadoutComponent.generated.h"

class UDLAbility;

UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLAbilityLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLAbilityLoadoutComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool LoadFromCharacter(const FDLCharacterAppearance& Character);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool TryGrenade();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool TryMelee();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool TryDash();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool TryShield();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool TryEvasion();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Ability")
	bool TrySuper();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Ability")
	EDLClassId GetClassId() const { return ClassId; }

	UDLAbility* GetSlot(EDLAbilitySlot Slot) const;

protected:
	bool TryActivate(EDLAbilitySlot Slot);
	void ClearSlots();

	UPROPERTY()
	EDLClassId ClassId = EDLClassId::Vanguard;

	UPROPERTY()
	TObjectPtr<UDLAbility> Grenade;

	UPROPERTY()
	TObjectPtr<UDLAbility> Shield;

	UPROPERTY()
	TObjectPtr<UDLAbility> Evasion;

	UPROPERTY()
	TObjectPtr<UDLAbility> Dash;

	UPROPERTY()
	TObjectPtr<UDLAbility> Melee;

	UPROPERTY()
	TObjectPtr<UDLAbility> Jump;

	UPROPERTY()
	TObjectPtr<UDLAbility> SuperAbility;
};
