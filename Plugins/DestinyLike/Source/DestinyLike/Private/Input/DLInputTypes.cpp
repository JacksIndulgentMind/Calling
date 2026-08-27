#include "Input/DLInputTypes.h"

bool FDLKeyChord::IsSet() const
{
	return Key.IsValid() && !DLInput::IsAltKey(Key);
}

bool FDLKeyChord::Equals(const FDLKeyChord& Other) const
{
	if (!IsSet() || !Other.IsSet())
	{
		return false;
	}
	return bAlt == Other.bAlt && DLInput::KeysMatch(Key, Other.Key);
}

FString FDLKeyChord::ToDisplayString() const
{
	if (!IsSet())
	{
		return TEXT("—");
	}
	FString KeyName;
	if (DLInput::IsMouseWheelKey(Key))
	{
		KeyName = TEXT("Mouse Wheel");
	}
	else
	{
		KeyName = Key.GetDisplayName().ToString();
	}
	return bAlt ? FString::Printf(TEXT("Alt + %s"), *KeyName) : KeyName;
}

namespace DLInput
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

	bool IsHoldAction(EDLBindableAction Action)
	{
		return Action == EDLBindableAction::Fire
			|| Action == EDLBindableAction::ADS
			|| Action == EDLBindableAction::Sprint
			|| Action == EDLBindableAction::Crouch;
	}

	bool IsExclusionCandidate(EDLBindableAction Action)
	{
		return Action == EDLBindableAction::Grenade
			|| Action == EDLBindableAction::Melee
			|| Action == EDLBindableAction::Dash
			|| Action == EDLBindableAction::Shield
			|| Action == EDLBindableAction::Evasion
			|| Action == EDLBindableAction::Dodge;
	}

	FName GetExclusionGroup(EDLBindableAction Action)
	{
		switch (Action)
		{
		case EDLBindableAction::Dash:
		case EDLBindableAction::Dodge:
		case EDLBindableAction::Evasion:
			return FName(TEXT("BodyCommit"));
		default:
			return NAME_None;
		}
	}

	FString ActionDisplayName(EDLBindableAction Action)
	{
		if (const UEnum* Enum = StaticEnum<EDLBindableAction>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Action)).ToString();
		}
		return ActionId(Action);
	}

	FString ColumnDisplayName(EDLBindColumn Column)
	{
		if (const UEnum* Enum = StaticEnum<EDLBindColumn>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Column)).ToString();
		}
		switch (Column)
		{
		case EDLBindColumn::Secondary: return TEXT("Secondary");
		case EDLBindColumn::Gamepad: return TEXT("Gamepad");
		default: return TEXT("Primary");
		}
	}

	FString ActionId(EDLBindableAction Action)
	{
		if (const UEnum* Enum = StaticEnum<EDLBindableAction>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Action));
		}
		return TEXT("Fire");
	}

	bool ActionFromId(const FString& Id, EDLBindableAction& OutAction)
	{
		if (const UEnum* Enum = StaticEnum<EDLBindableAction>())
		{
			const int64 Value = Enum->GetValueByNameString(Id);
			if (Value != INDEX_NONE)
			{
				OutAction = static_cast<EDLBindableAction>(Value);
				return true;
			}
		}
		return false;
	}

	const TArray<EDLBindableAction>& AllActions()
	{
		static const TArray<EDLBindableAction> Actions = {
			EDLBindableAction::Fire,
			EDLBindableAction::ADS,
			EDLBindableAction::Reload,
			EDLBindableAction::Swap,
			EDLBindableAction::WeaponPrimary,
			EDLBindableAction::WeaponSpecial,
			EDLBindableAction::Jump,
			EDLBindableAction::Sprint,
			EDLBindableAction::Crouch,
			EDLBindableAction::Slide,
			EDLBindableAction::AirDive,
			EDLBindableAction::Dodge,
			EDLBindableAction::Grenade,
			EDLBindableAction::Melee,
			EDLBindableAction::Dash,
			EDLBindableAction::Shield,
			EDLBindableAction::Evasion,
			EDLBindableAction::Super
		};
		return Actions;
	}
}
