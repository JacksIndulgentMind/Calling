#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loot/CLItemInstance.h"
#include "CLVaultSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCLOnLootEarned, const FCLItemInstance&, Item);

/**
 * Vault is the sole inventory. Drops land here and raise earn badges.
 * No separate player backpack.
 */
UCLASS()
class CALLING_API UCLVaultSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Calling|Vault")
	bool DepositItem(const FCLItemInstance& Item);

	UFUNCTION(BlueprintCallable, Category = "Calling|Vault")
	bool RemoveItem(const FGuid& InstanceId);

	UFUNCTION(BlueprintPure, Category = "Calling|Vault")
	TArray<FCLItemInstance> GetVaultItems() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Vault")
	bool FindItem(const FGuid& InstanceId, FCLItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, Category = "Calling|Vault")
	bool EquipWeapon(const FGuid& InstanceId);

	UFUNCTION(BlueprintPure, Category = "Calling|Vault")
	int32 GetUnreadEarnCount() const { return UnreadEarnBadges.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Calling|Vault")
	TArray<FCLItemInstance> ConsumeEarnBadges();

	UPROPERTY(BlueprintAssignable, Category = "Calling|Vault")
	FCLOnLootEarned OnLootEarned;

private:
	UPROPERTY()
	TArray<FCLItemInstance> UnreadEarnBadges;
};
