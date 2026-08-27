#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/CLTypes.h"
#include "Loot/CLItemInstance.h"
#include "CLLootRulesService.generated.h"

UENUM(BlueprintType)
enum class ECLWeaponFireMode : uint8
{
	Hitscan UMETA(DisplayName = "Hitscan"),
	Pellet UMETA(DisplayName = "Pellet"),
	Charge UMETA(DisplayName = "Charge"),
	Grenade UMETA(DisplayName = "Grenade"),
	Burst UMETA(DisplayName = "Burst")
};

UENUM(BlueprintType)
enum class ECLSightViewKind : uint8
{
	Iron UMETA(DisplayName = "Iron"),
	RedDot UMETA(DisplayName = "Red Dot"),
	Scope UMETA(DisplayName = "Scope")
};

UENUM(BlueprintType)
enum class ECLWeaponStock : uint8
{
	None UMETA(DisplayName = "None"),
	Brace UMETA(DisplayName = "Brace"),
	Stock UMETA(DisplayName = "Stock")
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponFireTune
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponFireMode Mode = ECLWeaponFireMode::Hitscan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 BurstCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 PelletCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float ChargeSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponClassDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponSlot Slot = ECLWeaponSlot::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AdsMovePenalty = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float HipFov = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AdsFov = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float HipSpreadDeg = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AdsSpreadDeg = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float CritMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponFireTune Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bInstantKillOnPrecision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float NoScopeReliability = 1.f;

	/** Default sight. Any sight is legal on any gun. Zoom FOV is the sight; Range is the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName SightId = FName(TEXT("red_dot"));

	/** Empty / tracer = hitscan plus optional tracer. grenade = real projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ProjectileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponStock Stock = ECLWeaponStock::None;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponMakeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ClassId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponStock Stock = ECLWeaponStock::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bHasStock = false;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLSightDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AdsFov = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AdsZoomSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLSightViewKind ViewKind = ECLSightViewKind::RedDot;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLModifierDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString Rarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName BehaviorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float BehaviorScale = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats StatDelta;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLDropRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLItemKind ItemKind = ECLItemKind::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLItemRarity Rarity = ECLItemRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<ECLWeaponSlot> Slots;

	/** When set, this roll always uses that weapon class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ClassId = NAME_None;

	/** Optional extra behavior stamped on the roll (e.g. prox_detonate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ForceBehaviorId = NAME_None;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLDropTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<FCLDropRoll> Rolls;
};

/**
 * Loads JSON loot configs and rolls drops locally. Hackable by design.
 */
UCLASS(BlueprintType)
class CALLING_API UCLLootRulesService : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Calling|Loot")
	bool LoadConfigs();

	UFUNCTION(BlueprintCallable, Category = "Calling|Loot")
	bool RollDrop(FName TableId, FCLItemInstance& OutItem) const;

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLWeaponClassDef> GetWeaponClasses() const { return WeaponClasses; }

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLModifierDef> GetModifierPool() const { return ModifierPool; }

	const FCLWeaponClassDef* FindWeaponClass(FName Id) const;
	const FCLWeaponMakeDef* FindWeaponMake(FName Id) const;
	const FCLSightDef* FindSight(FName Id) const;
	bool IsKnownSight(FName Id) const;

	/** Lookup table filled at LoadSights. Safe from view/HUD without a GameInstance. */
	static const FCLSightDef* FindLoadedSight(FName Id);
	static ECLSightViewKind SightViewKind(FName Id);

	FCLItemInstance MakeWeaponOfClass(FName ClassId, ECLItemRarity Rarity, const FString& TableId) const;

	void StampBehavior(FCLItemInstance& Item, FName BehaviorId, const FString& DisplayName) const;

private:
	bool LoadWeaponClasses(const FString& Path);
	bool LoadWeaponMakes(const FString& Path);
	bool LoadSights(const FString& Path);
	bool LoadModifierPool(const FString& Path);
	bool LoadDropTables(const FString& Path);

	FCLItemInstance MakeWeapon(ECLItemRarity Rarity, ECLWeaponSlot SlotFilter, const FString& TableId) const;
	FCLItemInstance MakeArmor(ECLItemRarity Rarity, const FString& TableId) const;
	TArray<FCLModifierRoll> RollModifiers(ECLItemRarity Rarity) const;
	static ECLItemRarity RarityFromString(const FString& S);
	static int32 ModifierCountForRarity(ECLItemRarity Rarity);

	UPROPERTY()
	TArray<FCLWeaponClassDef> WeaponClasses;

	UPROPERTY()
	TArray<FCLWeaponMakeDef> WeaponMakes;

	UPROPERTY()
	TArray<FCLSightDef> Sights;

	UPROPERTY()
	TArray<FCLModifierDef> ModifierPool;

	UPROPERTY()
	TArray<FCLDropTable> DropTables;

	UPROPERTY()
	float MaxStatDelta = 0.12f;

	UPROPERTY()
	float MaxBehaviorScale = 0.15f;
};
