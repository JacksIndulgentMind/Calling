#pragma once

#include "CoreMinimal.h"
#include "Core/DLTypes.h"
#include "DLItemInstance.generated.h"

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Impact = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Range = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Stability = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Handling = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Reload = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float FlinchResist = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AdsSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float MobilityBonus = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	int32 Magazine = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Rpm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float MassKg = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float BarrelLengthCm = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AmmoGrains = 62.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float MuzzleVelocityMps = 850.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Grip = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float Compensator = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float DrawSeconds = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float StowSeconds = 0.28f;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLModifierRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName ModifierId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName BehaviorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float BehaviorScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponStats StatDelta;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLWeaponIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLWeaponSlot Slot = EDLWeaponSlot::Primary;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLArmorIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLArmorPiece Piece = EDLArmorPiece::Helm;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLItemInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FGuid InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLItemKind Kind = EDLItemKind::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLItemRarity Rarity = EDLItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName DefinitionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponIdentity Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLArmorIdentity Armor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponStats BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponStats FinalStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	TArray<FDLModifierRoll> Modifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString SourceTableId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName RealmId = FName(TEXT("local"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName SightId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDateTime EarnedAt;

	void RecomputeFinalStats(float MaxStatDelta = 0.12f);
};
