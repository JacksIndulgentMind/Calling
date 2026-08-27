#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Loot/DLItemInstance.h"
#include "DLVaultSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLOnLootEarned, const FDLItemInstance&, Item);

/**
 * Vault is the sole inventory. Drops land here and raise earn badges.
 * No separate player backpack.
 */
UCLASS()
class DESTINYLIKE_API UDLVaultSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Vault")
	bool DepositItem(const FDLItemInstance& Item);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Vault")
	bool RemoveItem(const FGuid& InstanceId);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Vault")
	TArray<FDLItemInstance> GetVaultItems() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Vault")
	bool FindItem(const FGuid& InstanceId, FDLItemInstance& OutItem) const;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Vault")
	bool EquipWeapon(const FGuid& InstanceId);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Vault")
	int32 GetUnreadEarnCount() const { return UnreadEarnBadges.Num(); }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Vault")
	TArray<FDLItemInstance> ConsumeEarnBadges();

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Vault")
	FDLOnLootEarned OnLootEarned;

private:
	UPROPERTY()
	TArray<FDLItemInstance> UnreadEarnBadges;
};
