#include "Player/DLAbilityLoadoutComponent.h"
#include "Core/DLLog.h"
#include "Ability/DLAbility.h"
#include "Ability/DLAbilityCatalog.h"
#include "GameFramework/Pawn.h"

UDLAbilityLoadoutComponent::UDLAbilityLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDLAbilityLoadoutComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UDLAbility* Slots[] = { Grenade, Shield, Evasion, Dash, Melee, Jump, SuperAbility };
	for (UDLAbility* Ability : Slots)
	{
		if (Ability)
		{
			Ability->Tick(DeltaTime);
		}
	}
}

void UDLAbilityLoadoutComponent::ClearSlots()
{
	Grenade = nullptr;
	Shield = nullptr;
	Evasion = nullptr;
	Dash = nullptr;
	Melee = nullptr;
	Jump = nullptr;
	SuperAbility = nullptr;
}

bool UDLAbilityLoadoutComponent::LoadFromCharacter(const FDLCharacterAppearance& Character)
{
	ClearSlots();
	ClassId = Character.ClassId;
	UDLAbilityCatalog* Catalog = UDLAbilityCatalog::Get(this);
	if (!Catalog)
	{
		return false;
	}

	auto Bind = [&](EDLAbilitySlot Slot) -> UDLAbility*
	{
		return Catalog->SpawnBoundAbility(this, ClassId, Slot);
	};

	Grenade = Bind(EDLAbilitySlot::Grenade);
	Shield = Bind(EDLAbilitySlot::Shield);
	Evasion = Bind(EDLAbilitySlot::Evasion);
	Dash = Bind(EDLAbilitySlot::Dash);
	Melee = Bind(EDLAbilitySlot::Melee);
	Jump = Bind(EDLAbilitySlot::Jump);
	SuperAbility = Bind(EDLAbilitySlot::Super);

	if (APawn* Owner = Cast<APawn>(GetOwner()))
	{
		if (Jump)
		{
			Jump->ApplyToMovement(Owner);
		}
	}

	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike: loadout %d G=%s S=%s E=%s D=%s M=%s J=%s"),
		static_cast<int32>(ClassId),
		Grenade ? *Grenade->GetId().ToString() : TEXT("none"),
		Shield ? *Shield->GetId().ToString() : TEXT("none"),
		Evasion ? *Evasion->GetId().ToString() : TEXT("none"),
		Dash ? *Dash->GetId().ToString() : TEXT("none"),
		Melee ? *Melee->GetId().ToString() : TEXT("none"),
		Jump ? *Jump->GetId().ToString() : TEXT("none"));
	return Grenade && Shield && Evasion && Dash && Melee && Jump;
}

UDLAbility* UDLAbilityLoadoutComponent::GetSlot(EDLAbilitySlot Slot) const
{
	switch (Slot)
	{
	case EDLAbilitySlot::Grenade: return Grenade;
	case EDLAbilitySlot::Shield: return Shield;
	case EDLAbilitySlot::Evasion: return Evasion;
	case EDLAbilitySlot::Dash: return Dash;
	case EDLAbilitySlot::Melee: return Melee;
	case EDLAbilitySlot::Jump: return Jump;
	case EDLAbilitySlot::Super: return SuperAbility;
	default: return nullptr;
	}
}

bool UDLAbilityLoadoutComponent::TryActivate(EDLAbilitySlot Slot)
{
	UDLAbility* Ability = GetSlot(Slot);
	APawn* Owner = Cast<APawn>(GetOwner());
	return Ability && Ability->Activate(Owner);
}

bool UDLAbilityLoadoutComponent::TryGrenade() { return TryActivate(EDLAbilitySlot::Grenade); }
bool UDLAbilityLoadoutComponent::TryMelee() { return TryActivate(EDLAbilitySlot::Melee); }
bool UDLAbilityLoadoutComponent::TryDash() { return TryActivate(EDLAbilitySlot::Dash); }
bool UDLAbilityLoadoutComponent::TryShield() { return TryActivate(EDLAbilitySlot::Shield); }
bool UDLAbilityLoadoutComponent::TryEvasion() { return TryActivate(EDLAbilitySlot::Evasion); }
bool UDLAbilityLoadoutComponent::TrySuper() { return TryActivate(EDLAbilitySlot::Super); }
