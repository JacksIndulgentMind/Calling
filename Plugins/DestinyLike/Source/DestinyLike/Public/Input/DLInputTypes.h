#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "DLInputTypes.generated.h"

UENUM(BlueprintType)
enum class EDLBindableAction : uint8
{
	Fire UMETA(DisplayName = "Fire"),
	ADS UMETA(DisplayName = "ADS"),
	Reload UMETA(DisplayName = "Reload"),
	Swap UMETA(DisplayName = "Swap Weapon"),
	WeaponPrimary UMETA(DisplayName = "Weapon Primary"),
	WeaponSpecial UMETA(DisplayName = "Weapon Special"),
	Jump UMETA(DisplayName = "Jump"),
	Sprint UMETA(DisplayName = "Sprint"),
	Crouch UMETA(DisplayName = "Crouch"),
	Slide UMETA(DisplayName = "Slide"),
	AirDive UMETA(DisplayName = "Air Dive"),
	Dodge UMETA(DisplayName = "Dodge"),
	Grenade UMETA(DisplayName = "Grenade"),
	Melee UMETA(DisplayName = "Melee"),
	Dash UMETA(DisplayName = "Dash"),
	Shield UMETA(DisplayName = "Shield"),
	Evasion UMETA(DisplayName = "Evasion"),
	Super UMETA(DisplayName = "Super")
};

UENUM(BlueprintType)
enum class EDLBindColumn : uint8
{
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary"),
	Gamepad UMETA(DisplayName = "Gamepad")
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLKeyChord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	FKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	bool bAlt = false;

	bool IsSet() const;
	bool Equals(const FDLKeyChord& Other) const;
	FString ToDisplayString() const;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLActionBinds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	FDLKeyChord Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	FDLKeyChord Secondary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	FDLKeyChord Gamepad;

	bool IsUnbound() const { return !Primary.IsSet() && !Secondary.IsSet() && !Gamepad.IsSet(); }
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLBindUse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	EDLBindableAction Action = EDLBindableAction::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	EDLBindColumn Column = EDLBindColumn::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Input")
	bool bValid = false;
};

namespace DLInput
{
	DESTINYLIKE_API bool IsAltKey(const FKey& Key);
	DESTINYLIKE_API bool IsReservedMenuKey(const FKey& Key);
	DESTINYLIKE_API bool IsMouseWheelKey(const FKey& Key);
	DESTINYLIKE_API bool KeysMatch(const FKey& A, const FKey& B);
	DESTINYLIKE_API bool IsHoldAction(EDLBindableAction Action);
	DESTINYLIKE_API bool IsExclusionCandidate(EDLBindableAction Action);
	DESTINYLIKE_API FName GetExclusionGroup(EDLBindableAction Action);
	DESTINYLIKE_API FString ActionDisplayName(EDLBindableAction Action);
	DESTINYLIKE_API FString ColumnDisplayName(EDLBindColumn Column);
	DESTINYLIKE_API FString ActionId(EDLBindableAction Action);
	DESTINYLIKE_API bool ActionFromId(const FString& Id, EDLBindableAction& OutAction);
	DESTINYLIKE_API const TArray<EDLBindableAction>& AllActions();
}
