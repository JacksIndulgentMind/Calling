#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/DLTypes.h"
#include "Loot/DLItemInstance.h"
#include "DLLootRulesService.generated.h"

UENUM(BlueprintType)
enum class EDLWeaponFireMode : uint8
{
	Hitscan UMETA(DisplayName = "Hitscan"),
	Pellet UMETA(DisplayName = "Pellet"),
	Charge UMETA(DisplayName = "Charge"),
	Grenade UMETA(DisplayName = "Grenade")
};

UENUM(BlueprintType)
enum class EDLWeaponStock : uint8
{
	None UMETA(DisplayName = "None"),
	Brace UMETA(DisplayName = "Brace"),
	Stock UMETA(DisplayName = "Stock")
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLWeaponFireTune
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLWeaponFireMode Mode = EDLWeaponFireMode::Hitscan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	int32 BurstCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	int32 PelletCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float ChargeSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLWeaponClassDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLWeaponSlot Slot = EDLWeaponSlot::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponStats BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AdsMovePenalty = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float HipFov = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AdsFov = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float HipSpreadDeg = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AdsSpreadDeg = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float CritMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponFireTune Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	bool bInstantKillOnPrecision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float NoScopeReliability = 1.f;

	/** Default sight. Any sight is legal on any gun. Zoom FOV is the sight; Range is the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName SightId = FName(TEXT("red_dot"));

	/** Empty / tracer = hitscan plus optional tracer. grenade = real projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName ProjectileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLWeaponStock Stock = EDLWeaponStock::None;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLWeaponMakeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName ClassId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLWeaponStock Stock = EDLWeaponStock::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	bool bHasStock = false;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLSightDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AdsFov = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float AdsZoomSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLModifierDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName BehaviorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	float BehaviorScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FDLWeaponStats StatDelta;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLDropRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLItemKind ItemKind = EDLItemKind::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	EDLItemRarity Rarity = EDLItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	TArray<EDLWeaponSlot> Slots;

	/** When set, this roll always uses that weapon class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName ClassId = NAME_None;

	/** Optional extra behavior stamped on the roll (e.g. prox_detonate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName ForceBehaviorId = NAME_None;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLDropTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Loot")
	TArray<FDLDropRoll> Rolls;
};

/**
 * Loads JSON loot configs and rolls drops locally. Hackable by design.
 */
UCLASS(BlueprintType)
class DESTINYLIKE_API UDLLootRulesService : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Loot")
	bool LoadConfigs();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Loot")
	bool RollDrop(FName TableId, FDLItemInstance& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Loot")
	TArray<FDLWeaponClassDef> GetWeaponClasses() const { return WeaponClasses; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Loot")
	TArray<FDLModifierDef> GetModifierPool() const { return ModifierPool; }

	const FDLWeaponClassDef* FindWeaponClass(FName Id) const;
	const FDLWeaponMakeDef* FindWeaponMake(FName Id) const;
	const FDLSightDef* FindSight(FName Id) const;

	FDLItemInstance MakeWeaponOfClass(FName ClassId, EDLItemRarity Rarity, const FString& TableId) const;

	void StampBehavior(FDLItemInstance& Item, FName BehaviorId, const FString& DisplayName) const;

private:
	bool LoadWeaponClasses(const FString& Path);
	bool LoadWeaponMakes(const FString& Path);
	bool LoadSights(const FString& Path);
	bool LoadModifierPool(const FString& Path);
	bool LoadDropTables(const FString& Path);

	FDLItemInstance MakeWeapon(EDLItemRarity Rarity, EDLWeaponSlot SlotFilter, const FString& TableId) const;
	FDLItemInstance MakeArmor(EDLItemRarity Rarity, const FString& TableId) const;
	TArray<FDLModifierRoll> RollModifiers(EDLItemRarity Rarity) const;
	static EDLItemRarity RarityFromString(const FString& S);
	static int32 ModifierCountForRarity(EDLItemRarity Rarity);

	UPROPERTY()
	TArray<FDLWeaponClassDef> WeaponClasses;

	UPROPERTY()
	TArray<FDLWeaponMakeDef> WeaponMakes;

	UPROPERTY()
	TArray<FDLSightDef> Sights;

	UPROPERTY()
	TArray<FDLModifierDef> ModifierPool;

	UPROPERTY()
	TArray<FDLDropTable> DropTables;

	UPROPERTY()
	float MaxStatDelta = 0.12f;

	UPROPERTY()
	float MaxBehaviorScale = 0.15f;
};
