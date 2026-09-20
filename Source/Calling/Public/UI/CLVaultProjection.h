#pragma once

#include "CoreMinimal.h"
#include "Core/CLTypes.h"
#include "Loot/CLItemInstance.h"

class UCLLootRulesService;

enum class ECLVaultSort : uint8
{
	Newest,
	Name,
	Rarity,
	Class,
	EquippedFirst
};

struct FCLVaultRollRow
{
	FGuid InstanceId;
	FName DefinitionId = NAME_None;
	ECLWeaponSlot Slot = ECLWeaponSlot::Primary;
	ECLItemRarity Rarity = ECLItemRarity::Common;
	FCLWeaponStats FinalStats;
	TArray<FCLModifierRoll> Modifiers;
	FName RealmId = NAME_None;
	FDateTime EarnedAt;
	bool bEquipped = false;
	bool bEquippableThisMatch = true;
};

struct FCLVaultMakeGroup
{
	FName DefinitionId = NAME_None;
	FString DisplayName;
	FName ClassId = NAME_None;
	FName MakerId = NAME_None;
	ECLWeaponSlot Slot = ECLWeaponSlot::Primary;
	int32 RollCount = 0;
	ECLItemRarity BestRarity = ECLItemRarity::Common;
	FDateTime NewestEarnedAt;
	bool bContainsEquipped = false;
	bool bAnyEquippable = false;
	TArray<FCLVaultRollRow> Rolls;
};

struct FCLVaultFilter
{
	bool bSlot = false;
	ECLWeaponSlot Slot = ECLWeaponSlot::Primary;
	FName ClassId = NAME_None;
	FName MakerId = NAME_None;
	FString NameSubstr;
	bool bEquippableOnly = false;
};

namespace CLVaultProjection
{
	TArray<FCLVaultMakeGroup> ProjectWeapons(
		const TArray<FCLItemInstance>& VaultItems,
		const UCLLootRulesService* Loot,
		const FGuid& EquippedPrimary,
		const FGuid& EquippedSpecial,
		FName LiveRealm);

	TArray<FCLVaultMakeGroup> FilterGroups(const TArray<FCLVaultMakeGroup>& Groups, const FCLVaultFilter& Filter);
	void SortGroups(TArray<FCLVaultMakeGroup>& Groups, ECLVaultSort Sort);
	void SortRolls(TArray<FCLVaultRollRow>& Rolls, ECLVaultSort Sort);
}
