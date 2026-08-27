#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "CLInputTypes.generated.h"

UENUM(BlueprintType)
enum class ECLBindableAction : uint8
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
enum class ECLBindColumn : uint8
{
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary"),
	Gamepad UMETA(DisplayName = "Gamepad")
};

USTRUCT(BlueprintType)
struct CALLING_API FCLKeyChord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	FKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	bool bAlt = false;

	bool IsSet() const;
	bool Equals(const FCLKeyChord& Other) const;
	FString ToDisplayString() const;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLActionBinds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	FCLKeyChord Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	FCLKeyChord Secondary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	FCLKeyChord Gamepad;

	bool IsUnbound() const { return !Primary.IsSet() && !Secondary.IsSet() && !Gamepad.IsSet(); }
};

USTRUCT(BlueprintType)
struct CALLING_API FCLBindUse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	ECLBindableAction Action = ECLBindableAction::Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	ECLBindColumn Column = ECLBindColumn::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Input")
	bool bValid = false;
};

namespace CLInput
{
	CALLING_API bool IsAltKey(const FKey& Key);
	CALLING_API bool IsReservedMenuKey(const FKey& Key);
	CALLING_API bool IsMouseWheelKey(const FKey& Key);
	CALLING_API bool KeysMatch(const FKey& A, const FKey& B);
	CALLING_API bool IsHoldAction(ECLBindableAction Action);
	CALLING_API bool IsExclusionCandidate(ECLBindableAction Action);
	CALLING_API FName GetExclusionGroup(ECLBindableAction Action);
	CALLING_API FString ActionDisplayName(ECLBindableAction Action);
	CALLING_API FString ColumnDisplayName(ECLBindColumn Column);
	CALLING_API FString ActionId(ECLBindableAction Action);
	CALLING_API bool ActionFromId(const FString& Id, ECLBindableAction& OutAction);
	CALLING_API const TArray<ECLBindableAction>& AllActions();
}
