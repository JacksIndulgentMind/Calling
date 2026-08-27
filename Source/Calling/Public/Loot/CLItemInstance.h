#pragma once

#include "CoreMinimal.h"
#include "Core/CLTypes.h"
#include "CLItemInstance.generated.h"

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Impact = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Range = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Stability = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Handling = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Reload = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float FlinchResist = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AdsSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float MobilityBonus = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 Magazine = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Rpm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float MassKg = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float BarrelLengthCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AmmoGrains = 62.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float MuzzleVelocityMps = 850.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Grip = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float Compensator = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float DrawSeconds = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float StowSeconds = 0.28f;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLModifierRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName BehaviorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float BehaviorScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats StatDelta;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponSlot Slot = ECLWeaponSlot::Primary;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLArmorIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLArmorPiece Piece = ECLArmorPiece::Helm;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLItemKind Kind = ECLItemKind::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLItemRarity Rarity = ECLItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName DefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponIdentity Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLArmorIdentity Armor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats FinalStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<FCLModifierRoll> Modifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString SourceTableId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName RealmId = FName(TEXT("local"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName SightId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FDateTime EarnedAt;

	void RecomputeFinalStats(float MaxStatDelta = 0.12f);
};
