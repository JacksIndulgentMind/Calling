#include "Player/CLAbilityLoadoutComponent.h"
#include "Core/CLLog.h"
#include "Ability/CLAbility.h"
#include "Ability/CLAbilityCatalog.h"
#include "GameFramework/Pawn.h"

UCLAbilityLoadoutComponent::UCLAbilityLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCLAbilityLoadoutComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UCLAbility* Slots[] = { Grenade, Shield, Evasion, Dash, Melee, Jump, SuperAbility };
	for (UCLAbility* Ability : Slots)
	{
		if (Ability)
		{
			Ability->Tick(DeltaTime);
		}
	}
}

void UCLAbilityLoadoutComponent::ClearSlots()
{
	Grenade = nullptr;
	Shield = nullptr;
	Evasion = nullptr;
	Dash = nullptr;
	Melee = nullptr;
	Jump = nullptr;
	SuperAbility = nullptr;
}

bool UCLAbilityLoadoutComponent::LoadFromCharacter(const FCLCharacterAppearance& Character)
{
	ClearSlots();
	ClassId = Character.ClassId;
	UCLAbilityCatalog* Catalog = UCLAbilityCatalog::Get(this);
	if (!Catalog)
	{
		return false;
	}

	auto Bind = [&](ECLAbilitySlot Slot) -> UCLAbility*
	{
		return Catalog->SpawnBoundAbility(this, ClassId, Slot);
	};

	Grenade = Bind(ECLAbilitySlot::Grenade);
	Shield = Bind(ECLAbilitySlot::Shield);
	Evasion = Bind(ECLAbilitySlot::Evasion);
	Dash = Bind(ECLAbilitySlot::Dash);
	Melee = Bind(ECLAbilitySlot::Melee);
	Jump = Bind(ECLAbilitySlot::Jump);
	SuperAbility = Bind(ECLAbilitySlot::Super);

	if (APawn* Owner = Cast<APawn>(GetOwner()))
	{
		if (Jump)
		{
			Jump->ApplyToMovement(Owner);
		}
	}

	UE_LOG(LogCalling, Display, TEXT("Calling: loadout %d G=%s S=%s E=%s D=%s M=%s J=%s"),
		static_cast<int32>(ClassId),
		Grenade ? *Grenade->GetId().ToString() : TEXT("none"),
		Shield ? *Shield->GetId().ToString() : TEXT("none"),
		Evasion ? *Evasion->GetId().ToString() : TEXT("none"),
		Dash ? *Dash->GetId().ToString() : TEXT("none"),
		Melee ? *Melee->GetId().ToString() : TEXT("none"),
		Jump ? *Jump->GetId().ToString() : TEXT("none"));
	return Grenade && Shield && Evasion && Dash && Melee && Jump;
}

UCLAbility* UCLAbilityLoadoutComponent::GetSlot(ECLAbilitySlot Slot) const
{
	switch (Slot)
	{
	case ECLAbilitySlot::Grenade: return Grenade;
	case ECLAbilitySlot::Shield: return Shield;
	case ECLAbilitySlot::Evasion: return Evasion;
	case ECLAbilitySlot::Dash: return Dash;
	case ECLAbilitySlot::Melee: return Melee;
	case ECLAbilitySlot::Jump: return Jump;
	case ECLAbilitySlot::Super: return SuperAbility;
	default: return nullptr;
	}
}

bool UCLAbilityLoadoutComponent::TryActivate(ECLAbilitySlot Slot)
{
	UCLAbility* Ability = GetSlot(Slot);
	APawn* Owner = Cast<APawn>(GetOwner());
	return Ability && Ability->Activate(Owner);
}

bool UCLAbilityLoadoutComponent::TryGrenade() { return TryActivate(ECLAbilitySlot::Grenade); }
bool UCLAbilityLoadoutComponent::TryMelee() { return TryActivate(ECLAbilitySlot::Melee); }
bool UCLAbilityLoadoutComponent::TryDash() { return TryActivate(ECLAbilitySlot::Dash); }
bool UCLAbilityLoadoutComponent::TryShield() { return TryActivate(ECLAbilitySlot::Shield); }
bool UCLAbilityLoadoutComponent::TryEvasion() { return TryActivate(ECLAbilitySlot::Evasion); }
bool UCLAbilityLoadoutComponent::TrySuper() { return TryActivate(ECLAbilitySlot::Super); }
