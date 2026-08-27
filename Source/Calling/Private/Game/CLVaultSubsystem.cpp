#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLGameInstance.h"
#include "Game/CLLobbySubsystem.h"

bool UCLVaultSubsystem::DepositItem(const FCLItemInstance& Item)
{
	UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>();
	if (!Profiles || !Profiles->HasActiveProfile())
	{
		return false;
	}

	FCLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId);
	if (!Profile)
	{
		return false;
	}

	FCLItemInstance Copy = Item;
	if (!Copy.InstanceId.IsValid())
	{
		Copy.InstanceId = FGuid::NewGuid();
	}
	if (Copy.RealmId.IsNone())
	{
		Copy.RealmId = FName(TEXT("local"));
	}
	if (Copy.EarnedAt.GetTicks() == 0)
	{
		Copy.EarnedAt = FDateTime::UtcNow();
	}

	Profile->VaultItems.Add(Copy);
	Profiles->SaveActiveProfile();

	UnreadEarnBadges.Add(Copy);
	OnLootEarned.Broadcast(Copy);
	return true;
}

bool UCLVaultSubsystem::RemoveItem(const FGuid& InstanceId)
{
	UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>();
	if (!Profiles || !Profiles->HasActiveProfile())
	{
		return false;
	}

	FCLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId);
	if (!Profile)
	{
		return false;
	}

	const int32 Index = Profile->VaultItems.IndexOfByPredicate([&](const FCLItemInstance& It)
	{
		return It.InstanceId == InstanceId;
	});
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Profile->VaultItems.RemoveAt(Index);
	if (Profile->EquippedPrimaryId == InstanceId) Profile->EquippedPrimaryId.Invalidate();
	if (Profile->EquippedSpecialId == InstanceId) Profile->EquippedSpecialId.Invalidate();
	Profiles->SaveActiveProfile();
	return true;
}

TArray<FCLItemInstance> UCLVaultSubsystem::GetVaultItems() const
{
	if (const UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
	{
		return Profiles->GetActiveProfile().VaultItems;
	}
	return {};
}

bool UCLVaultSubsystem::FindItem(const FGuid& InstanceId, FCLItemInstance& OutItem) const
{
	for (const FCLItemInstance& Item : GetVaultItems())
	{
		if (Item.InstanceId == InstanceId)
		{
			OutItem = Item;
			return true;
		}
	}
	return false;
}

bool UCLVaultSubsystem::EquipWeapon(const FGuid& InstanceId)
{
	UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>();
	if (!Profiles || !Profiles->HasActiveProfile())
	{
		return false;
	}

	FCLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId);
	if (!Profile)
	{
		return false;
	}

	const FCLItemInstance* Found = Profile->VaultItems.FindByPredicate([&](const FCLItemInstance& It)
	{
		return It.InstanceId == InstanceId && It.Kind == ECLItemKind::Weapon;
	});
	if (!Found)
	{
		return false;
	}

	FName RealmId = FName(TEXT("local"));
	if (UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>())
	{
		RealmId = Lobby->GetLootRealmId();
	}
	const FName ItemRealm = Found->RealmId.IsNone() ? FName(TEXT("local")) : Found->RealmId;
	if (ItemRealm != RealmId)
	{
		return false;
	}

	if (Found->Weapon.Slot == ECLWeaponSlot::Primary)
	{
		Profile->EquippedPrimaryId = InstanceId;
	}
	else
	{
		Profile->EquippedSpecialId = InstanceId;
	}

	Profiles->SaveActiveProfile();
	return true;
}

TArray<FCLItemInstance> UCLVaultSubsystem::ConsumeEarnBadges()
{
	TArray<FCLItemInstance> Copy = UnreadEarnBadges;
	UnreadEarnBadges.Reset();
	return Copy;
}
