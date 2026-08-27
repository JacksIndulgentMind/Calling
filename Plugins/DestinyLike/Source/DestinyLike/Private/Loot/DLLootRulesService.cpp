#include "Loot/DLLootRulesService.h"
#include "Core/DLLog.h"
#include "Core/DLError.h"
#include "Game/DLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Interfaces/IPluginManager.h"

namespace
{
	FString ResolvePluginConfigPath(const FString& RelativeUnderPlugin)
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DestinyLike")))
		{
			return FPaths::Combine(Plugin->GetBaseDir(), RelativeUnderPlugin);
		}
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("DestinyLike"), RelativeUnderPlugin);
	}

	EDLWeaponSlot SlotFromString(const FString& S)
	{
		return S.Equals(TEXT("Special"), ESearchCase::IgnoreCase) ? EDLWeaponSlot::Special : EDLWeaponSlot::Primary;
	}

	EDLWeaponStock StockFromString(const FString& S)
	{
		if (S.Equals(TEXT("brace"), ESearchCase::IgnoreCase))
		{
			return EDLWeaponStock::Brace;
		}
		if (S.Equals(TEXT("stock"), ESearchCase::IgnoreCase))
		{
			return EDLWeaponStock::Stock;
		}
		return EDLWeaponStock::None;
	}

	void ReadPhysicalStats(const TSharedPtr<FJsonObject>& O, FDLWeaponStats& Stats)
	{
		auto Num = [&](const TCHAR* Key, float& Out)
		{
			if (O->HasField(Key))
			{
				Out = static_cast<float>(O->GetNumberField(Key));
			}
		};
		Num(TEXT("massKg"), Stats.MassKg);
		Num(TEXT("barrelLengthCm"), Stats.BarrelLengthCm);
		Num(TEXT("ammoGrains"), Stats.AmmoGrains);
		Num(TEXT("muzzleVelocityMps"), Stats.MuzzleVelocityMps);
		Num(TEXT("grip"), Stats.Grip);
		Num(TEXT("compensator"), Stats.Compensator);
		Num(TEXT("drawSeconds"), Stats.DrawSeconds);
		Num(TEXT("stowSeconds"), Stats.StowSeconds);
	}
}

bool UDLLootRulesService::LoadConfigs()
{
	WeaponClasses.Reset();
	WeaponMakes.Reset();
	Sights.Reset();
	ModifierPool.Reset();
	DropTables.Reset();

	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxModifierStatDelta"), MaxStatDelta, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxModifierBehaviorScale"), MaxBehaviorScale, GGameIni);

	const bool bWeapons = LoadWeaponClasses(ResolvePluginConfigPath(TEXT("Config/Loot/WeaponClasses.json")));
	const bool bMakes = LoadWeaponMakes(ResolvePluginConfigPath(TEXT("Config/Loot/WeaponMakes.json")));
	const bool bSights = LoadSights(ResolvePluginConfigPath(TEXT("Config/Loot/Sights.json")));
	const bool bMods = LoadModifierPool(ResolvePluginConfigPath(TEXT("Config/Loot/ModifierPool.json")));
	const bool bTables = LoadDropTables(ResolvePluginConfigPath(TEXT("Config/Loot/DropTables.json")));
	if (!(bWeapons && bMakes && bSights && bMods && bTables))
	{
		UDLErrorBoundary::ReportStatic(this, FDLError::Make(
			EDLErrorKind::NonDeterministic,
			TEXT("loot_config"),
			TEXT("Loot JSON failed to load")));
		return false;
	}
	return true;
}

bool UDLLootRulesService::LoadWeaponClasses(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: missing WeaponClasses.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weaponClasses"), Arr) || !Arr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;

		FDLWeaponClassDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->GetStringField(TEXT("displayName"));
		Def.Slot = SlotFromString(O->GetStringField(TEXT("slot")));
		Def.BaseStats.Rpm = O->GetNumberField(TEXT("rpm"));
		Def.BaseStats.Impact = O->GetNumberField(TEXT("baseImpact"));
		Def.BaseStats.Range = O->GetNumberField(TEXT("baseRange"));
		Def.BaseStats.Stability = O->GetNumberField(TEXT("baseStability"));
		Def.BaseStats.Handling = O->GetNumberField(TEXT("baseHandling"));
		Def.BaseStats.Reload = O->GetNumberField(TEXT("baseReload"));
		Def.BaseStats.Magazine = static_cast<int32>(O->GetNumberField(TEXT("magazine")));
		Def.AdsMovePenalty = O->GetNumberField(TEXT("adsMovePenalty"));
		Def.HipFov = O->GetNumberField(TEXT("hipFov"));
		Def.AdsFov = O->HasField(TEXT("adsFov")) ? O->GetNumberField(TEXT("adsFov")) : 70.f;
		Def.HipSpreadDeg = O->HasField(TEXT("hipSpreadDeg")) ? O->GetNumberField(TEXT("hipSpreadDeg")) : 1.2f;
		Def.AdsSpreadDeg = O->HasField(TEXT("adsSpreadDeg")) ? O->GetNumberField(TEXT("adsSpreadDeg")) : 0.18f;
		Def.CritMultiplier = O->GetNumberField(TEXT("critMultiplier"));
		Def.Fire.BurstCount = O->HasField(TEXT("burstCount")) ? static_cast<int32>(O->GetNumberField(TEXT("burstCount"))) : 1;
		Def.Fire.PelletCount = O->HasField(TEXT("pelletCount")) ? static_cast<int32>(O->GetNumberField(TEXT("pelletCount"))) : 1;
		Def.Fire.ChargeSeconds = O->HasField(TEXT("chargeSeconds")) ? O->GetNumberField(TEXT("chargeSeconds")) : 0.f;
		Def.bInstantKillOnPrecision = O->HasField(TEXT("instantKillOnPrecision")) && O->GetBoolField(TEXT("instantKillOnPrecision"));
		Def.NoScopeReliability = O->HasField(TEXT("noScopeReliability")) ? O->GetNumberField(TEXT("noScopeReliability")) : 1.f;
		if (O->HasField(TEXT("sightId")))
		{
			Def.SightId = FName(*O->GetStringField(TEXT("sightId")));
		}
		if (O->HasField(TEXT("projectile")))
		{
			Def.ProjectileId = FName(*O->GetStringField(TEXT("projectile")));
		}
		if (O->HasField(TEXT("stock")))
		{
			Def.Stock = StockFromString(O->GetStringField(TEXT("stock")));
		}
		ReadPhysicalStats(O, Def.BaseStats);
		if (Def.ProjectileId == FName(TEXT("grenade")))
		{
			Def.Fire.Mode = EDLWeaponFireMode::Grenade;
		}
		else if (Def.Fire.PelletCount > 1)
		{
			Def.Fire.Mode = EDLWeaponFireMode::Pellet;
		}
		else if (Def.Fire.ChargeSeconds > 0.f)
		{
			Def.Fire.Mode = EDLWeaponFireMode::Charge;
		}
		else
		{
			Def.Fire.Mode = EDLWeaponFireMode::Hitscan;
		}
		WeaponClasses.Add(Def);
	}
	return WeaponClasses.Num() > 0;
}

bool UDLLootRulesService::LoadWeaponMakes(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: missing WeaponMakes.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weaponMakes"), Arr) || !Arr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid())
		{
			continue;
		}
		FDLWeaponMakeDef Make;
		Make.Id = FName(*O->GetStringField(TEXT("id")));
		Make.ClassId = FName(*O->GetStringField(TEXT("classId")));
		Make.DisplayName = O->GetStringField(TEXT("displayName"));
		if (O->HasField(TEXT("stock")))
		{
			Make.Stock = StockFromString(O->GetStringField(TEXT("stock")));
			Make.bHasStock = true;
		}
		ReadPhysicalStats(O, Make.Stats);
		WeaponMakes.Add(Make);

		const FDLWeaponClassDef* Base = FindWeaponClass(Make.ClassId);
		if (!Base)
		{
			continue;
		}
		FDLWeaponClassDef Baked = *Base;
		Baked.Id = Make.Id;
		Baked.DisplayName = Make.DisplayName;
		if (Make.bHasStock)
		{
			Baked.Stock = Make.Stock;
		}
		auto Overlay = [](float Src, float& Dst)
		{
			if (Src > 0.f)
			{
				Dst = Src;
			}
		};
		Overlay(Make.Stats.MassKg, Baked.BaseStats.MassKg);
		Overlay(Make.Stats.BarrelLengthCm, Baked.BaseStats.BarrelLengthCm);
		Overlay(Make.Stats.AmmoGrains, Baked.BaseStats.AmmoGrains);
		Overlay(Make.Stats.MuzzleVelocityMps, Baked.BaseStats.MuzzleVelocityMps);
		if (Make.Stats.Grip > 0.f) { Baked.BaseStats.Grip = Make.Stats.Grip; }
		Baked.BaseStats.Compensator = Make.Stats.Compensator;
		Overlay(Make.Stats.DrawSeconds, Baked.BaseStats.DrawSeconds);
		Overlay(Make.Stats.StowSeconds, Baked.BaseStats.StowSeconds);
		WeaponClasses.Add(Baked);
	}
	return WeaponMakes.Num() > 0;
}

bool UDLLootRulesService::LoadSights(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: missing Sights.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("sights"), Arr) || !Arr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FDLSightDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->HasField(TEXT("displayName")) ? O->GetStringField(TEXT("displayName")) : Def.Id.ToString();
		Def.AdsFov = O->HasField(TEXT("adsFov")) ? O->GetNumberField(TEXT("adsFov")) : 70.f;
		Def.AdsZoomSeconds = O->HasField(TEXT("adsZoomSeconds")) ? O->GetNumberField(TEXT("adsZoomSeconds")) : 0.f;
		Sights.Add(Def);
	}
	return Sights.Num() > 0;
}

bool UDLLootRulesService::LoadModifierPool(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	if (Root->HasField(TEXT("maxStatDelta")))
	{
		MaxStatDelta = Root->GetNumberField(TEXT("maxStatDelta"));
	}
	if (Root->HasField(TEXT("maxBehaviorScale")))
	{
		MaxBehaviorScale = Root->GetNumberField(TEXT("maxBehaviorScale"));
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("modifiers"), Arr) || !Arr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;

		FDLModifierDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->GetStringField(TEXT("displayName"));
		Def.Rarity = O->GetStringField(TEXT("rarity"));
		Def.Type = O->GetStringField(TEXT("type"));
		if (O->HasField(TEXT("behaviorId")))
		{
			Def.BehaviorId = FName(*O->GetStringField(TEXT("behaviorId")));
		}
		if (O->HasField(TEXT("behaviorScale")))
		{
			Def.BehaviorScale = FMath::Clamp(static_cast<float>(O->GetNumberField(TEXT("behaviorScale"))), 0.f, MaxBehaviorScale);
		}
		if (const TSharedPtr<FJsonObject> Stats = O->GetObjectField(TEXT("stats")))
		{
			auto Read = [&](const TCHAR* Key, float& Out)
			{
				if (Stats->HasField(Key))
				{
					Out = FMath::Clamp(static_cast<float>(Stats->GetNumberField(Key)), -MaxStatDelta, MaxStatDelta);
				}
			};
			Read(TEXT("stability"), Def.StatDelta.Stability);
			Read(TEXT("handling"), Def.StatDelta.Handling);
			Read(TEXT("range"), Def.StatDelta.Range);
			Read(TEXT("reload"), Def.StatDelta.Reload);
			Read(TEXT("flinchResist"), Def.StatDelta.FlinchResist);
			Read(TEXT("adsSpeed"), Def.StatDelta.AdsSpeed);
			Read(TEXT("mobilityBonus"), Def.StatDelta.MobilityBonus);
		}
		ModifierPool.Add(Def);
	}
	return ModifierPool.Num() > 0;
}

bool UDLLootRulesService::LoadDropTables(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("tables"), Arr) || !Arr)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;

		FDLDropTable Table;
		Table.Id = FName(*O->GetStringField(TEXT("id")));
		const TArray<TSharedPtr<FJsonValue>>* Rolls = nullptr;
		if (O->TryGetArrayField(TEXT("rolls"), Rolls) && Rolls)
		{
			for (const TSharedPtr<FJsonValue>& RV : *Rolls)
			{
				const TSharedPtr<FJsonObject> R = RV->AsObject();
				if (!R.IsValid()) continue;
				FDLDropRoll Roll;
				Roll.ItemKind = R->GetStringField(TEXT("itemKind")).Equals(TEXT("armor"), ESearchCase::IgnoreCase)
					? EDLItemKind::Armor : EDLItemKind::Weapon;
				Roll.Rarity = RarityFromString(R->GetStringField(TEXT("rarity")));
				Roll.Weight = static_cast<int32>(R->GetNumberField(TEXT("weight")));
				const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
				if (R->TryGetArrayField(TEXT("slots"), Slots) && Slots)
				{
					for (const TSharedPtr<FJsonValue>& SV : *Slots)
					{
						Roll.Slots.Add(SlotFromString(SV->AsString()));
					}
				}
				if (R->HasField(TEXT("classId")))
				{
					Roll.ClassId = FName(*R->GetStringField(TEXT("classId")));
				}
				if (R->HasField(TEXT("forceBehavior")))
				{
					Roll.ForceBehaviorId = FName(*R->GetStringField(TEXT("forceBehavior")));
				}
				Table.Rolls.Add(Roll);
			}
		}
		DropTables.Add(Table);
	}
	return DropTables.Num() > 0;
}

EDLItemRarity UDLLootRulesService::RarityFromString(const FString& S)
{
	if (S.Equals(TEXT("uncommon"), ESearchCase::IgnoreCase)) return EDLItemRarity::Uncommon;
	if (S.Equals(TEXT("rare"), ESearchCase::IgnoreCase)) return EDLItemRarity::Rare;
	if (S.Equals(TEXT("epic"), ESearchCase::IgnoreCase)) return EDLItemRarity::Epic;
	if (S.Equals(TEXT("legendary"), ESearchCase::IgnoreCase)) return EDLItemRarity::Legendary;
	if (S.Equals(TEXT("exotic"), ESearchCase::IgnoreCase)) return EDLItemRarity::Exotic;
	return EDLItemRarity::Common;
}

int32 UDLLootRulesService::ModifierCountForRarity(EDLItemRarity Rarity)
{
	switch (Rarity)
	{
	case EDLItemRarity::Common: return 1;
	case EDLItemRarity::Uncommon: return 2;
	case EDLItemRarity::Rare: return 3;
	default: return 4;
	}
}

const FDLWeaponClassDef* UDLLootRulesService::FindWeaponClass(FName Id) const
{
	return WeaponClasses.FindByPredicate([&](const FDLWeaponClassDef& D) { return D.Id == Id; });
}

const FDLWeaponMakeDef* UDLLootRulesService::FindWeaponMake(FName Id) const
{
	return WeaponMakes.FindByPredicate([&](const FDLWeaponMakeDef& D) { return D.Id == Id; });
}

const FDLSightDef* UDLLootRulesService::FindSight(FName Id) const
{
	if (Id.IsNone())
	{
		Id = FName(TEXT("red_dot"));
	}
	if (const FDLSightDef* Found = Sights.FindByPredicate([&](const FDLSightDef& D) { return D.Id == Id; }))
	{
		return Found;
	}
	return Sights.Num() > 0 ? &Sights[0] : nullptr;
}

bool UDLLootRulesService::RollDrop(FName TableId, FDLItemInstance& OutItem) const
{
	const FDLDropTable* Table = DropTables.FindByPredicate([&](const FDLDropTable& T) { return T.Id == TableId; });
	if (!Table || Table->Rolls.Num() == 0)
	{
		return false;
	}

	int32 TotalWeight = 0;
	for (const FDLDropRoll& R : Table->Rolls)
	{
		TotalWeight += FMath::Max(1, R.Weight);
	}

	int32 Pick = FMath::RandRange(1, TotalWeight);
	const FDLDropRoll* Chosen = &Table->Rolls[0];
	for (const FDLDropRoll& R : Table->Rolls)
	{
		Pick -= FMath::Max(1, R.Weight);
		if (Pick <= 0)
		{
			Chosen = &R;
			break;
		}
	}

	if (Chosen->ItemKind == EDLItemKind::Armor)
	{
		OutItem = MakeArmor(Chosen->Rarity, TableId.ToString());
	}
	else if (!Chosen->ClassId.IsNone())
	{
		OutItem = MakeWeaponOfClass(Chosen->ClassId, Chosen->Rarity, TableId.ToString());
	}
	else
	{
		EDLWeaponSlot Slot = EDLWeaponSlot::Primary;
		if (Chosen->Slots.Num() > 0)
		{
			Slot = Chosen->Slots[FMath::RandRange(0, Chosen->Slots.Num() - 1)];
		}
		OutItem = MakeWeapon(Chosen->Rarity, Slot, TableId.ToString());
	}
	if (!Chosen->ForceBehaviorId.IsNone() && OutItem.Kind == EDLItemKind::Weapon)
	{
		StampBehavior(OutItem, Chosen->ForceBehaviorId, Chosen->ForceBehaviorId.ToString());
	}
	return true;
}

TArray<FDLModifierRoll> UDLLootRulesService::RollModifiers(EDLItemRarity Rarity) const
{
	TArray<FDLModifierRoll> Result;
	if (ModifierPool.Num() == 0)
	{
		return Result;
	}

	const int32 Count = FMath::Min(ModifierCountForRarity(Rarity), 4);
	TArray<int32> Used;
	for (int32 i = 0; i < Count; ++i)
	{
		int32 Index = FMath::RandRange(0, ModifierPool.Num() - 1);
		int32 Guard = 0;
		while (Used.Contains(Index) && Guard++ < 16)
		{
			Index = FMath::RandRange(0, ModifierPool.Num() - 1);
		}
		Used.AddUnique(Index);

		const FDLModifierDef& Def = ModifierPool[Index];
		FDLModifierRoll Roll;
		Roll.ModifierId = Def.Id;
		Roll.DisplayName = Def.DisplayName;
		Roll.BehaviorId = Def.BehaviorId;
		Roll.BehaviorScale = FMath::Clamp(Def.BehaviorScale, 0.f, MaxBehaviorScale);
		Roll.StatDelta = Def.StatDelta;
		Result.Add(Roll);
	}
	return Result;
}

FDLItemInstance UDLLootRulesService::MakeWeapon(EDLItemRarity Rarity, EDLWeaponSlot SlotFilter, const FString& TableId) const
{
	TArray<const FDLWeaponClassDef*> Candidates;
	for (const FDLWeaponClassDef& Def : WeaponClasses)
	{
		if (Def.Slot == SlotFilter)
		{
			Candidates.Add(&Def);
		}
	}
	if (Candidates.Num() == 0)
	{
		for (const FDLWeaponClassDef& Def : WeaponClasses)
		{
			Candidates.Add(&Def);
		}
	}

	const FDLWeaponClassDef* ClassDef = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	return MakeWeaponOfClass(ClassDef->Id, Rarity, TableId);
}

FDLItemInstance UDLLootRulesService::MakeWeaponOfClass(FName ClassId, EDLItemRarity Rarity, const FString& TableId) const
{
	const FDLWeaponClassDef* ClassDef = FindWeaponClass(ClassId);
	if (!ClassDef && WeaponClasses.Num() > 0)
	{
		ClassDef = &WeaponClasses[0];
	}

	FDLItemInstance Item;
	Item.InstanceId = FGuid::NewGuid();
	Item.Kind = EDLItemKind::Weapon;
	Item.Rarity = Rarity;
	if (ClassDef)
	{
		Item.DefinitionId = ClassDef->Id;
		Item.DisplayName = ClassDef->DisplayName;
		Item.Weapon.Slot = ClassDef->Slot;
		Item.BaseStats = ClassDef->BaseStats;
		Item.SightId = ClassDef->SightId;
	}
	else
	{
		Item.DefinitionId = ClassId;
		Item.DisplayName = ClassId.ToString();
	}
	if (!TableId.Equals(TEXT("starter")))
	{
		Item.Modifiers = RollModifiers(Rarity);
		Item.BaseStats.Grip = FMath::Clamp(Item.BaseStats.Grip + FMath::FRandRange(-0.08f, 0.08f), 0.15f, 1.f);
		Item.BaseStats.MassKg = FMath::Max(0.25f, Item.BaseStats.MassKg * FMath::FRandRange(0.94f, 1.06f));
		Item.BaseStats.BarrelLengthCm = FMath::Max(6.f, Item.BaseStats.BarrelLengthCm * FMath::FRandRange(0.96f, 1.04f));
	}
	Item.SourceTableId = TableId;
	Item.RealmId = FName(TEXT("local"));
	Item.EarnedAt = FDateTime::UtcNow();
	Item.RecomputeFinalStats(MaxStatDelta);
	return Item;
}

void UDLLootRulesService::StampBehavior(FDLItemInstance& Item, FName BehaviorId, const FString& DisplayName) const
{
	if (BehaviorId.IsNone())
	{
		return;
	}

	for (const FDLModifierRoll& Existing : Item.Modifiers)
	{
		if (Existing.BehaviorId == BehaviorId)
		{
			return;
		}
	}

	FDLModifierRoll Roll;
	if (const FDLModifierDef* Def = ModifierPool.FindByPredicate(
		[&](const FDLModifierDef& M) { return M.BehaviorId == BehaviorId; }))
	{
		Roll.ModifierId = Def->Id;
		Roll.DisplayName = Def->DisplayName;
		Roll.BehaviorId = Def->BehaviorId;
		Roll.BehaviorScale = Def->BehaviorScale;
		Roll.StatDelta = Def->StatDelta;
	}
	else
	{
		Roll.ModifierId = BehaviorId;
		Roll.DisplayName = DisplayName;
		Roll.BehaviorId = BehaviorId;
	}
	Item.Modifiers.Add(Roll);
	Item.RecomputeFinalStats(MaxStatDelta);
}

FDLItemInstance UDLLootRulesService::MakeArmor(EDLItemRarity Rarity, const FString& TableId) const
{
	static const EDLArmorPiece Pieces[] = {
		EDLArmorPiece::Helm, EDLArmorPiece::Arms, EDLArmorPiece::Chest, EDLArmorPiece::Legs
	};

	FDLItemInstance Item;
	Item.InstanceId = FGuid::NewGuid();
	Item.Kind = EDLItemKind::Armor;
	Item.Rarity = Rarity;
	Item.Armor.Piece = Pieces[FMath::RandRange(0, 3)];
	Item.DefinitionId = FName(*FString::Printf(TEXT("armor_%d"), static_cast<int32>(Item.Armor.Piece)));
	Item.DisplayName = FString::Printf(TEXT("Armor Piece %d"), static_cast<int32>(Item.Armor.Piece));
	Item.BaseStats.Handling = 0.05f * (static_cast<int32>(Rarity) + 1);
	Item.BaseStats.MobilityBonus = 0.02f * (static_cast<int32>(Rarity) + 1);
	Item.FinalStats = Item.BaseStats;
	Item.SourceTableId = TableId;
	Item.RealmId = FName(TEXT("local"));
	Item.EarnedAt = FDateTime::UtcNow();
	return Item;
}
