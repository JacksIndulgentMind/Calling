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

	/** Class band (`pistol`, `rifle`, …). Equals `Id` on a raw class; stays the band after a make overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName BandId = NAME_None;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponMakerDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	/** Internal lineage. Not HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString InspiredBy;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponCaliberDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Band = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float AmmoGrains = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float MuzzleVelocityMps = 0.f;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponSocketDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Socket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Mesh = FName(TEXT("cube"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FVector Loc = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FVector Scale = FVector(0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FRotator Rot = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponFrameDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ClassId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FVector Muzzle = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FVector Ejector = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<FCLWeaponSocketDef> Visuals;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponPartDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Slot = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<FName> AllowedClassIds;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponMakeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName MakerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ClassId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName FrameId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName CaliberId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	/** Internal lineage. Not HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString InspiredBy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLWeaponStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponStock Stock = ECLWeaponStock::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bHasStock = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bHasMagazine = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bHasRpm = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bHasFireMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLWeaponFireMode FireMode = ECLWeaponFireMode::Hitscan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 BurstCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<FName> AllowedPartSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString Thumb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString Concept;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bWorldDrop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bWorldOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName PrimarySourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName FactionId = NAME_None;
};

UENUM(BlueprintType)
enum class ECLDropSourceKind : uint8
{
	RaidBoss UMETA(DisplayName = "Raid Boss"),
	RaidMob UMETA(DisplayName = "Raid Mob"),
	PvpAward UMETA(DisplayName = "PvP Award"),
	PvpComplete UMETA(DisplayName = "PvP Complete"),
	World UMETA(DisplayName = "World"),
	FactionVendor UMETA(DisplayName = "Faction Vendor")
};

USTRUCT(BlueprintType)
struct CALLING_API FCLDropSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	ECLDropSourceKind Kind = ECLDropSourceKind::World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName ActivityId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString ActivityName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName EncounterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString EncounterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName NodeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString NodeName;

	FString PathLabel() const
	{
		TArray<FString> Parts;
		if (!ActivityName.IsEmpty())
		{
			Parts.Add(ActivityName);
		}
		if (!EncounterName.IsEmpty())
		{
			Parts.Add(EncounterName);
		}
		if (!NodeName.IsEmpty())
		{
			Parts.Add(NodeName);
		}
		FString Label;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			if (i > 0)
			{
				Label += TEXT(" / ");
			}
			Label += Parts[i];
		}
		return Label;
	}
};

USTRUCT(BlueprintType)
struct CALLING_API FCLDropVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = FName(TEXT("prestige"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	float StatBandScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString Thumb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString Concept;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bBranded = true;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLFactionDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Kind = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName VendorTableId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	TArray<FName> UniformSlots;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponSourceRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName TableId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName MakeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLDropSource Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLDropVariant Variant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 WeaponWeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	int32 TableWeightTotal = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName FactionId = NAME_None;

	float AdvertisedRate() const
	{
		return TableWeightTotal > 0 ? static_cast<float>(WeaponWeight) / static_cast<float>(TableWeightTotal) : 0.f;
	}
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

	/** Pin this roll to one make. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName MakeId = NAME_None;

	/** `world` = any world-drop make. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Pool = NAME_None;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLDropTable
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLDropSource Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	bool bHasSource = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FCLDropVariant Variant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Loot")
	FName FactionId = NAME_None;

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
	TArray<FCLWeaponMakeDef> GetWeaponMakes() const { return WeaponMakes; }

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLWeaponFrameDef> GetWeaponFrames() const { return WeaponFrames; }

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLModifierDef> GetModifierPool() const { return ModifierPool; }

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLWeaponMakerDef> GetWeaponMakers() const { return WeaponMakers; }

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLFactionDef> GetFactions() const { return Factions; }

	UFUNCTION(BlueprintPure, Category = "Calling|Loot")
	TArray<FCLWeaponPartDef> GetWeaponParts() const { return WeaponParts; }

	const FCLWeaponClassDef* FindWeaponClass(FName Id) const;
	const FCLWeaponMakeDef* FindWeaponMake(FName Id) const;
	const FCLWeaponMakerDef* FindWeaponMaker(FName Id) const;
	const FCLWeaponCaliberDef* FindWeaponCaliber(FName Id) const;
	const FCLWeaponFrameDef* FindWeaponFrame(FName Id) const;
	const FCLWeaponFrameDef* FindWeaponFrameForClass(FName ClassId) const;
	const FCLFactionDef* FindFaction(FName Id) const;
	const FCLDropTable* FindDropTable(FName Id) const;
	const FCLSightDef* FindSight(FName Id) const;
	bool IsKnownSight(FName Id) const;

	TArray<FCLWeaponMakeDef> MakesMatching(FName ClassId, FName MakerId) const;
	TArray<FCLWeaponSourceRef> SourcesForMake(FName MakeId) const;
	FCLWeaponSourceRef PrimarySourceForMake(FName MakeId) const;
	void StatBandFor(FName MakeId, const FCLWeaponSourceRef& Source, FCLWeaponStats& OutMin, FCLWeaponStats& OutMax) const;
	TArray<FCLModifierDef> ModsForMake(FName MakeId) const;
	TArray<FCLWeaponPartDef> PartsForMake(FName MakeId) const;
	float MaxStatDeltaValue() const { return MaxStatDelta; }

	/** Old Destiny family ids remap into the compact class list. */
	static FName CanonicalWeaponClassId(FName Id);

	/** Fill `Out` with class band + make overlay. `DefinitionId` may be a make or a class. */
	bool ComposeEquippedClass(FName DefinitionId, FCLWeaponClassDef& Out) const;

	/** Lookup table filled at LoadSights. Safe from view/HUD without a GameInstance. */
	static const FCLSightDef* FindLoadedSight(FName Id);
	static ECLSightViewKind SightViewKind(FName Id);

	FCLItemInstance MakeWeaponOfClass(FName ClassId, ECLItemRarity Rarity, const FString& TableId) const;

	void StampBehavior(FCLItemInstance& Item, FName BehaviorId, const FString& DisplayName) const;

private:
	bool LoadWeaponClasses(const FString& Path);
	bool LoadWeaponMakers(const FString& Path);
	bool LoadWeaponCalibers(const FString& Path);
	bool LoadWeaponFrames(const FString& Path);
	bool LoadWeaponParts(const FString& Path);
	bool LoadWeaponMakes(const FString& Path);
	bool LoadSights(const FString& Path);
	bool LoadModifierPool(const FString& Path);
	bool LoadDropTables(const FString& Path);
	bool LoadFactions(const FString& Path);
	void BuildMakeSourceIndex();

	FCLItemInstance MakeWeapon(ECLItemRarity Rarity, ECLWeaponSlot SlotFilter, const FString& TableId) const;
	FCLItemInstance MakeArmor(ECLItemRarity Rarity, const FString& TableId) const;
	FCLItemInstance StampWeaponFromMake(const FCLWeaponMakeDef& Make, ECLItemRarity Rarity, const FString& TableId, float StatBandScale = 1.f) const;
	const FCLWeaponMakeDef* PickMakeForClass(FName ClassId) const;
	const FCLWeaponMakeDef* PickWorldMake() const;
	void ApplyMakeOverlay(FCLWeaponClassDef& ClassDef, const FCLWeaponMakeDef& Make) const;
	TArray<FCLModifierRoll> RollModifiers(ECLItemRarity Rarity, float StatBandScale = 1.f) const;
	static ECLItemRarity RarityFromString(const FString& S);
	static int32 ModifierCountForRarity(ECLItemRarity Rarity);
	static ECLDropSourceKind SourceKindFromString(const FString& S);
	static ECLItemKind ItemKindFromString(const FString& S);

	UPROPERTY()
	TArray<FCLWeaponClassDef> WeaponClasses;

	UPROPERTY()
	TArray<FCLWeaponMakerDef> WeaponMakers;

	UPROPERTY()
	TArray<FCLWeaponCaliberDef> WeaponCalibers;

	UPROPERTY()
	TArray<FCLWeaponFrameDef> WeaponFrames;

	UPROPERTY()
	TArray<FCLWeaponPartDef> WeaponParts;

	UPROPERTY()
	TArray<FCLWeaponMakeDef> WeaponMakes;

	UPROPERTY()
	TArray<FCLSightDef> Sights;

	UPROPERTY()
	TArray<FCLModifierDef> ModifierPool;

	UPROPERTY()
	TArray<FCLDropTable> DropTables;

	UPROPERTY()
	TArray<FCLFactionDef> Factions;

	TMap<FName, TArray<FCLWeaponSourceRef>> MakeSources;

	UPROPERTY()
	float MaxStatDelta = 0.12f;

	UPROPERTY()
	float MaxBehaviorScale = 0.15f;
};
