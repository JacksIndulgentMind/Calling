#include "Input/CLInputTypes.h"

bool FCLKeyChord::IsSet() const
{
	return Key.IsValid() && !CLInput::IsAltKey(Key);
}

bool FCLKeyChord::Equals(const FCLKeyChord& Other) const
{
	if (!IsSet() || !Other.IsSet())
	{
		return false;
	}
	return bAlt == Other.bAlt && CLInput::KeysMatch(Key, Other.Key);
}

FString FCLKeyChord::ToDisplayString() const
{
	if (!IsSet())
	{
		return TEXT("—");
	}
	FString KeyName;
	if (CLInput::IsMouseWheelKey(Key))
	{
		KeyName = TEXT("Mouse Wheel");
	}
	else
	{
		KeyName = Key.GetDisplayName().ToString();
	}
	return bAlt ? FString::Printf(TEXT("Alt + %s"), *KeyName) : KeyName;
}

namespace CLInput
{
	bool IsAltKey(const FKey& Key)
	{
		return Key == EKeys::LeftAlt || Key == EKeys::RightAlt;
	}

	bool IsReservedMenuKey(const FKey& Key)
	{
		return Key == EKeys::Escape || Key == EKeys::F1 || Key == EKeys::I
			|| Key == EKeys::Gamepad_Special_Right;
	}

	bool IsMouseWheelKey(const FKey& Key)
	{
		return Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown;
	}

	bool KeysMatch(const FKey& A, const FKey& B)
	{
		if (IsMouseWheelKey(A) && IsMouseWheelKey(B))
		{
			return true;
		}
		return A == B;
	}

	bool IsHoldAction(ECLBindableAction Action)
	{
		return Action == ECLBindableAction::Fire
			|| Action == ECLBindableAction::ADS
			|| Action == ECLBindableAction::Sprint
			|| Action == ECLBindableAction::Crouch;
	}

	bool IsExclusionCandidate(ECLBindableAction Action)
	{
		return Action == ECLBindableAction::Grenade
			|| Action == ECLBindableAction::Melee
			|| Action == ECLBindableAction::Dash
			|| Action == ECLBindableAction::Shield
			|| Action == ECLBindableAction::Evasion
			|| Action == ECLBindableAction::Dodge;
	}

	FName GetExclusionGroup(ECLBindableAction Action)
	{
		switch (Action)
		{
		case ECLBindableAction::Dash:
		case ECLBindableAction::Dodge:
		case ECLBindableAction::Evasion:
			return FName(TEXT("BodyCommit"));
		default:
			return NAME_None;
		}
	}

	FString ActionDisplayName(ECLBindableAction Action)
	{
		if (const UEnum* Enum = StaticEnum<ECLBindableAction>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Action)).ToString();
		}
		return ActionId(Action);
	}

	FString ColumnDisplayName(ECLBindColumn Column)
	{
		if (const UEnum* Enum = StaticEnum<ECLBindColumn>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Column)).ToString();
		}
		switch (Column)
		{
		case ECLBindColumn::Secondary: return TEXT("Secondary");
		case ECLBindColumn::Gamepad: return TEXT("Gamepad");
		default: return TEXT("Primary");
		}
	}

	FString ActionId(ECLBindableAction Action)
	{
		if (const UEnum* Enum = StaticEnum<ECLBindableAction>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Action));
		}
		return TEXT("Fire");
	}

	bool ActionFromId(const FString& Id, ECLBindableAction& OutAction)
	{
		if (const UEnum* Enum = StaticEnum<ECLBindableAction>())
		{
			const int64 Value = Enum->GetValueByNameString(Id);
			if (Value != INDEX_NONE)
			{
				OutAction = static_cast<ECLBindableAction>(Value);
				return true;
			}
		}
		return false;
	}

	const TArray<ECLBindableAction>& AllActions()
	{
		static const TArray<ECLBindableAction> Actions = {
			ECLBindableAction::Fire,
			ECLBindableAction::ADS,
			ECLBindableAction::Reload,
			ECLBindableAction::Swap,
			ECLBindableAction::WeaponPrimary,
			ECLBindableAction::WeaponSpecial,
			ECLBindableAction::Jump,
			ECLBindableAction::Sprint,
			ECLBindableAction::Crouch,
			ECLBindableAction::Slide,
			ECLBindableAction::AirDive,
			ECLBindableAction::Dodge,
			ECLBindableAction::Grenade,
			ECLBindableAction::Melee,
			ECLBindableAction::Dash,
			ECLBindableAction::Shield,
			ECLBindableAction::Evasion,
			ECLBindableAction::Super
		};
		return Actions;
	}
}
