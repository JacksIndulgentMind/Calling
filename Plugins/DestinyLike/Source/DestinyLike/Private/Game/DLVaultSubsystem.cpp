#include "Game/DLVaultSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLGameInstance.h"
#include "Game/DLLobbySubsystem.h"

bool UDLVaultSubsystem::DepositItem(const FDLItemInstance& Item)
{
	UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>();
	if (!Profiles || !Profiles->HasActiveProfile())
	{
		return false;
	}

	FDLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId);
	if (!Profile)
	{
		return false;
	}

	FDLItemInstance Copy = Item;
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

bool UDLVaultSubsystem::RemoveItem(const FGuid& InstanceId)
{
	UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>();
	if (!Profiles || !Profiles->HasActiveProfile())
	{
		return false;
	}

	FDLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId);
	if (!Profile)
	{
		return false;
	}

	const int32 Index = Profile->VaultItems.IndexOfByPredicate([&](const FDLItemInstance& It)
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

TArray<FDLItemInstance> UDLVaultSubsystem::GetVaultItems() const
{
	if (const UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>())
	{
		return Profiles->GetActiveProfile().VaultItems;
	}
	return {};
}

bool UDLVaultSubsystem::FindItem(const FGuid& InstanceId, FDLItemInstance& OutItem) const
{
	for (const FDLItemInstance& Item : GetVaultItems())
	{
		if (Item.InstanceId == InstanceId)
		{
			OutItem = Item;
			return true;
		}
	}
	return false;
}

bool UDLVaultSubsystem::EquipWeapon(const FGuid& InstanceId)
{
	UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>();
	if (!Profiles || !Profiles->HasActiveProfile())
	{
		return false;
	}

	FDLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId);
	if (!Profile)
	{
		return false;
	}

	const FDLItemInstance* Found = Profile->VaultItems.FindByPredicate([&](const FDLItemInstance& It)
	{
		return It.InstanceId == InstanceId && It.Kind == EDLItemKind::Weapon;
	});
	if (!Found)
	{
		return false;
	}

	FName RealmId = FName(TEXT("local"));
	if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
	{
		RealmId = Lobby->GetLootRealmId();
	}
	const FName ItemRealm = Found->RealmId.IsNone() ? FName(TEXT("local")) : Found->RealmId;
	if (ItemRealm != RealmId)
	{
		return false;
	}

	if (Found->Weapon.Slot == EDLWeaponSlot::Primary)
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

TArray<FDLItemInstance> UDLVaultSubsystem::ConsumeEarnBadges()
{
	TArray<FDLItemInstance> Copy = UnreadEarnBadges;
	UnreadEarnBadges.Reset();
	return Copy;
}
