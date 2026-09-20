#include "UI/CLVaultProjection.h"
#include "Loot/CLLootRulesService.h"

namespace
{
	FName NormalizeRealm(FName Realm)
	{
		return Realm.IsNone() ? FName(TEXT("local")) : Realm;
	}

	int32 RarityRank(ECLItemRarity Rarity)
	{
		return static_cast<int32>(Rarity);
	}

	bool NameContains(const FString& Hay, const FString& Needle)
	{
		if (Needle.IsEmpty())
		{
			return true;
		}
		return Hay.Contains(Needle, ESearchCase::IgnoreCase);
	}
}

TArray<FCLVaultMakeGroup> CLVaultProjection::ProjectWeapons(
	const TArray<FCLItemInstance>& VaultItems,
	const UCLLootRulesService* Loot,
	const FGuid& EquippedPrimary,
	const FGuid& EquippedSpecial,
	FName LiveRealm)
{
	TMap<FName, int32> Index;
	TArray<FCLVaultMakeGroup> Groups;
	const FName Live = NormalizeRealm(LiveRealm);

	for (const FCLItemInstance& Item : VaultItems)
	{
		if (Item.Kind != ECLItemKind::Weapon)
		{
			continue;
		}

		FCLVaultRollRow Row;
		Row.InstanceId = Item.InstanceId;
		Row.DefinitionId = Item.DefinitionId;
		Row.Slot = Item.Weapon.Slot;
		Row.Rarity = Item.Rarity;
		Row.FinalStats = Item.FinalStats;
		Row.Modifiers = Item.Modifiers;
		Row.RealmId = NormalizeRealm(Item.RealmId);
		Row.EarnedAt = Item.EarnedAt;
		Row.bEquipped = Item.InstanceId == EquippedPrimary || Item.InstanceId == EquippedSpecial;
		Row.bEquippableThisMatch = Row.RealmId == Live;

		const int32* Found = Index.Find(Item.DefinitionId);
		if (!Found)
		{
			FCLVaultMakeGroup Group;
			Group.DefinitionId = Item.DefinitionId;
			if (Loot)
			{
				if (const FCLWeaponMakeDef* Make = Loot->FindWeaponMake(Item.DefinitionId))
				{
					Group.DisplayName = Make->DisplayName;
					Group.ClassId = Make->ClassId;
					Group.MakerId = Make->MakerId;
				}
			}
			if (Group.DisplayName.IsEmpty())
			{
				Group.DisplayName = Item.DisplayName;
			}
			Group.Slot = Item.Weapon.Slot;
			Index.Add(Item.DefinitionId, Groups.Add(Group));
			Found = Index.Find(Item.DefinitionId);
		}

		FCLVaultMakeGroup& Group = Groups[*Found];
		Group.Rolls.Add(Row);
		Group.RollCount = Group.Rolls.Num();
		if (RarityRank(Row.Rarity) > RarityRank(Group.BestRarity))
		{
			Group.BestRarity = Row.Rarity;
		}
		if (Row.EarnedAt > Group.NewestEarnedAt)
		{
			Group.NewestEarnedAt = Row.EarnedAt;
		}
		if (Row.bEquipped)
		{
			Group.bContainsEquipped = true;
		}
		if (Row.bEquippableThisMatch)
		{
			Group.bAnyEquippable = true;
		}
	}

	return Groups;
}

TArray<FCLVaultMakeGroup> CLVaultProjection::FilterGroups(const TArray<FCLVaultMakeGroup>& Groups, const FCLVaultFilter& Filter)
{
	TArray<FCLVaultMakeGroup> Out;
	for (const FCLVaultMakeGroup& Group : Groups)
	{
		if (Filter.bSlot && Group.Slot != Filter.Slot)
		{
			continue;
		}
		if (!Filter.ClassId.IsNone() && Group.ClassId != Filter.ClassId)
		{
			continue;
		}
		if (!Filter.MakerId.IsNone() && Group.MakerId != Filter.MakerId)
		{
			continue;
		}
		if (!NameContains(Group.DisplayName, Filter.NameSubstr))
		{
			continue;
		}
		if (Filter.bEquippableOnly && !Group.bAnyEquippable)
		{
			continue;
		}
		Out.Add(Group);
	}
	return Out;
}

void CLVaultProjection::SortGroups(TArray<FCLVaultMakeGroup>& Groups, ECLVaultSort Sort)
{
	Groups.Sort([Sort](const FCLVaultMakeGroup& A, const FCLVaultMakeGroup& B)
	{
		switch (Sort)
		{
		case ECLVaultSort::Name:
			return A.DisplayName < B.DisplayName;
		case ECLVaultSort::Rarity:
			if (RarityRank(A.BestRarity) != RarityRank(B.BestRarity))
			{
				return RarityRank(A.BestRarity) > RarityRank(B.BestRarity);
			}
			return A.DisplayName < B.DisplayName;
		case ECLVaultSort::Class:
			if (A.ClassId != B.ClassId)
			{
				return A.ClassId.LexicalLess(B.ClassId);
			}
			return A.DisplayName < B.DisplayName;
		case ECLVaultSort::EquippedFirst:
			if (A.bContainsEquipped != B.bContainsEquipped)
			{
				return A.bContainsEquipped && !B.bContainsEquipped;
			}
			return A.NewestEarnedAt > B.NewestEarnedAt;
		default:
			return A.NewestEarnedAt > B.NewestEarnedAt;
		}
	});
}

void CLVaultProjection::SortRolls(TArray<FCLVaultRollRow>& Rolls, ECLVaultSort Sort)
{
	Rolls.Sort([Sort](const FCLVaultRollRow& A, const FCLVaultRollRow& B)
	{
		if (Sort == ECLVaultSort::Rarity)
		{
			if (RarityRank(A.Rarity) != RarityRank(B.Rarity))
			{
				return RarityRank(A.Rarity) > RarityRank(B.Rarity);
			}
		}
		if (Sort == ECLVaultSort::EquippedFirst)
		{
			if (A.bEquipped != B.bEquipped)
			{
				return A.bEquipped && !B.bEquipped;
			}
		}
		return A.EarnedAt > B.EarnedAt;
	});
}
