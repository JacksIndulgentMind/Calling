#include "Loot/CLLootRulesService.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString ResolveProjectConfigPath(const FString& RelativeUnderConfig)
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), RelativeUnderConfig);
	}

	ECLWeaponSlot SlotFromString(const FString& S)
	{
		return S.Equals(TEXT("Special"), ESearchCase::IgnoreCase) ? ECLWeaponSlot::Special : ECLWeaponSlot::Primary;
	}

	ECLWeaponStock StockFromString(const FString& S)
	{
		if (S.Equals(TEXT("brace"), ESearchCase::IgnoreCase))
		{
			return ECLWeaponStock::Brace;
		}
		if (S.Equals(TEXT("stock"), ESearchCase::IgnoreCase))
		{
			return ECLWeaponStock::Stock;
		}
		return ECLWeaponStock::None;
	}

	bool FireModeFromString(const FString& S, ECLWeaponFireMode& Out)
	{
		if (S.Equals(TEXT("hitscan"), ESearchCase::IgnoreCase)) { Out = ECLWeaponFireMode::Hitscan; return true; }
		if (S.Equals(TEXT("pellet"), ESearchCase::IgnoreCase)) { Out = ECLWeaponFireMode::Pellet; return true; }
		if (S.Equals(TEXT("charge"), ESearchCase::IgnoreCase)) { Out = ECLWeaponFireMode::Charge; return true; }
		if (S.Equals(TEXT("grenade"), ESearchCase::IgnoreCase)) { Out = ECLWeaponFireMode::Grenade; return true; }
		if (S.Equals(TEXT("burst"), ESearchCase::IgnoreCase)) { Out = ECLWeaponFireMode::Burst; return true; }
		return false;
	}

	ECLWeaponFireMode HeuristicFireMode(const FCLWeaponClassDef& Def)
	{
		if (Def.ProjectileId == FName(TEXT("grenade")))
		{
			return ECLWeaponFireMode::Grenade;
		}
		if (Def.Fire.PelletCount > 1)
		{
			return ECLWeaponFireMode::Pellet;
		}
		if (Def.Fire.ChargeSeconds > 0.f)
		{
			return ECLWeaponFireMode::Charge;
		}
		if (Def.Fire.BurstCount > 1)
		{
			return ECLWeaponFireMode::Burst;
		}
		return ECLWeaponFireMode::Hitscan;
	}

	ECLSightViewKind SightViewKindFrom(const FString& S, FName Id)
	{
		if (S.Equals(TEXT("iron"), ESearchCase::IgnoreCase) || Id == FName(TEXT("iron")))
		{
			return ECLSightViewKind::Iron;
		}
		if (S.Equals(TEXT("scope"), ESearchCase::IgnoreCase) || Id == FName(TEXT("scope")))
		{
			return ECLSightViewKind::Scope;
		}
		return ECLSightViewKind::RedDot;
	}

	TMap<FName, FCLSightDef>& LoadedSightTable()
	{
		static TMap<FName, FCLSightDef> Table;
		return Table;
	}

	void ReadPhysicalStats(const TSharedPtr<FJsonObject>& O, FCLWeaponStats& Stats)
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

bool UCLLootRulesService::LoadConfigs()
{
	WeaponClasses.Reset();
	WeaponMakes.Reset();
	Sights.Reset();
	ModifierPool.Reset();
	DropTables.Reset();

	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxModifierStatDelta"), MaxStatDelta, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxModifierBehaviorScale"), MaxBehaviorScale, GGameIni);

	const bool bWeapons = LoadWeaponClasses(ResolveProjectConfigPath(TEXT("Loot/WeaponClasses.json")));
	const bool bMakes = LoadWeaponMakes(ResolveProjectConfigPath(TEXT("Loot/WeaponMakes.json")));
	const bool bSights = LoadSights(ResolveProjectConfigPath(TEXT("Loot/Sights.json")));
	const bool bMods = LoadModifierPool(ResolveProjectConfigPath(TEXT("Loot/ModifierPool.json")));
	const bool bTables = LoadDropTables(ResolveProjectConfigPath(TEXT("Loot/DropTables.json")));
	if (!(bWeapons && bMakes && bSights && bMods && bTables))
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("loot_config"),
			TEXT("Loot JSON failed to load")));
		return false;
	}
	return true;
}

bool UCLLootRulesService::LoadWeaponClasses(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing WeaponClasses.json at %s"), *Path);
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

		FCLWeaponClassDef Def;
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
		FString FireModeName;
		if (!(O->TryGetStringField(TEXT("fireMode"), FireModeName) && FireModeFromString(FireModeName, Def.Fire.Mode)))
		{
			Def.Fire.Mode = HeuristicFireMode(Def);
		}
		WeaponClasses.Add(Def);
	}
	return WeaponClasses.Num() > 0;
}

bool UCLLootRulesService::LoadWeaponMakes(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing WeaponMakes.json at %s"), *Path);
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
		FCLWeaponMakeDef Make;
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

		const FCLWeaponClassDef* Base = FindWeaponClass(Make.ClassId);
		if (!Base)
		{
			continue;
		}
		FCLWeaponClassDef Baked = *Base;
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

bool UCLLootRulesService::LoadSights(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing Sights.json at %s"), *Path);
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

	LoadedSightTable().Reset();
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) continue;
		FCLSightDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->HasField(TEXT("displayName")) ? O->GetStringField(TEXT("displayName")) : Def.Id.ToString();
		Def.AdsFov = O->HasField(TEXT("adsFov")) ? O->GetNumberField(TEXT("adsFov")) : 70.f;
		Def.AdsZoomSeconds = O->HasField(TEXT("adsZoomSeconds")) ? O->GetNumberField(TEXT("adsZoomSeconds")) : 0.f;
		FString ViewKindName;
		O->TryGetStringField(TEXT("viewKind"), ViewKindName);
		Def.ViewKind = SightViewKindFrom(ViewKindName, Def.Id);
		Sights.Add(Def);
		LoadedSightTable().Add(Def.Id, Def);
	}
	return Sights.Num() > 0;
}

bool UCLLootRulesService::LoadModifierPool(const FString& Path)
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

		FCLModifierDef Def;
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

bool UCLLootRulesService::LoadDropTables(const FString& Path)
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

		FCLDropTable Table;
		Table.Id = FName(*O->GetStringField(TEXT("id")));
		const TArray<TSharedPtr<FJsonValue>>* Rolls = nullptr;
		if (O->TryGetArrayField(TEXT("rolls"), Rolls) && Rolls)
		{
			for (const TSharedPtr<FJsonValue>& RV : *Rolls)
			{
				const TSharedPtr<FJsonObject> R = RV->AsObject();
				if (!R.IsValid()) continue;
				FCLDropRoll Roll;
				Roll.ItemKind = R->GetStringField(TEXT("itemKind")).Equals(TEXT("armor"), ESearchCase::IgnoreCase)
					? ECLItemKind::Armor : ECLItemKind::Weapon;
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

ECLItemRarity UCLLootRulesService::RarityFromString(const FString& S)
{
	if (S.Equals(TEXT("uncommon"), ESearchCase::IgnoreCase)) return ECLItemRarity::Uncommon;
	if (S.Equals(TEXT("rare"), ESearchCase::IgnoreCase)) return ECLItemRarity::Rare;
	if (S.Equals(TEXT("epic"), ESearchCase::IgnoreCase)) return ECLItemRarity::Epic;
	if (S.Equals(TEXT("legendary"), ESearchCase::IgnoreCase)) return ECLItemRarity::Legendary;
	if (S.Equals(TEXT("exotic"), ESearchCase::IgnoreCase)) return ECLItemRarity::Exotic;
	return ECLItemRarity::Common;
}

int32 UCLLootRulesService::ModifierCountForRarity(ECLItemRarity Rarity)
{
	switch (Rarity)
	{
	case ECLItemRarity::Common: return 1;
	case ECLItemRarity::Uncommon: return 2;
	case ECLItemRarity::Rare: return 3;
	default: return 4;
	}
}

const FCLWeaponClassDef* UCLLootRulesService::FindWeaponClass(FName Id) const
{
	return WeaponClasses.FindByPredicate([&](const FCLWeaponClassDef& D) { return D.Id == Id; });
}

const FCLWeaponMakeDef* UCLLootRulesService::FindWeaponMake(FName Id) const
{
	return WeaponMakes.FindByPredicate([&](const FCLWeaponMakeDef& D) { return D.Id == Id; });
}

const FCLSightDef* UCLLootRulesService::FindSight(FName Id) const
{
	if (Id.IsNone())
	{
		Id = FName(TEXT("red_dot"));
	}
	if (const FCLSightDef* Found = Sights.FindByPredicate([&](const FCLSightDef& D) { return D.Id == Id; }))
	{
		return Found;
	}
	return Sights.Num() > 0 ? &Sights[0] : nullptr;
}

bool UCLLootRulesService::IsKnownSight(FName Id) const
{
	return Sights.ContainsByPredicate([&](const FCLSightDef& D) { return D.Id == Id; });
}

const FCLSightDef* UCLLootRulesService::FindLoadedSight(FName Id)
{
	if (Id.IsNone())
	{
		Id = FName(TEXT("red_dot"));
	}
	return LoadedSightTable().Find(Id);
}

ECLSightViewKind UCLLootRulesService::SightViewKind(FName Id)
{
	if (const FCLSightDef* Found = FindLoadedSight(Id))
	{
		return Found->ViewKind;
	}
	return SightViewKindFrom(FString(), Id);
}

bool UCLLootRulesService::RollDrop(FName TableId, FCLItemInstance& OutItem) const
{
	const FCLDropTable* Table = DropTables.FindByPredicate([&](const FCLDropTable& T) { return T.Id == TableId; });
	if (!Table || Table->Rolls.Num() == 0)
	{
		return false;
	}

	int32 TotalWeight = 0;
	for (const FCLDropRoll& R : Table->Rolls)
	{
		TotalWeight += FMath::Max(1, R.Weight);
	}

	int32 Pick = FMath::RandRange(1, TotalWeight);
	const FCLDropRoll* Chosen = &Table->Rolls[0];
	for (const FCLDropRoll& R : Table->Rolls)
	{
		Pick -= FMath::Max(1, R.Weight);
		if (Pick <= 0)
		{
			Chosen = &R;
			break;
		}
	}

	if (Chosen->ItemKind == ECLItemKind::Armor)
	{
		OutItem = MakeArmor(Chosen->Rarity, TableId.ToString());
	}
	else if (!Chosen->ClassId.IsNone())
	{
		OutItem = MakeWeaponOfClass(Chosen->ClassId, Chosen->Rarity, TableId.ToString());
	}
	else
	{
		ECLWeaponSlot Slot = ECLWeaponSlot::Primary;
		if (Chosen->Slots.Num() > 0)
		{
			Slot = Chosen->Slots[FMath::RandRange(0, Chosen->Slots.Num() - 1)];
		}
		OutItem = MakeWeapon(Chosen->Rarity, Slot, TableId.ToString());
	}
	if (!Chosen->ForceBehaviorId.IsNone() && OutItem.Kind == ECLItemKind::Weapon)
	{
		StampBehavior(OutItem, Chosen->ForceBehaviorId, Chosen->ForceBehaviorId.ToString());
	}
	return true;
}

TArray<FCLModifierRoll> UCLLootRulesService::RollModifiers(ECLItemRarity Rarity) const
{
	TArray<FCLModifierRoll> Result;
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

		const FCLModifierDef& Def = ModifierPool[Index];
		FCLModifierRoll Roll;
		Roll.ModifierId = Def.Id;
		Roll.DisplayName = Def.DisplayName;
		Roll.BehaviorId = Def.BehaviorId;
		Roll.BehaviorScale = FMath::Clamp(Def.BehaviorScale, 0.f, MaxBehaviorScale);
		Roll.StatDelta = Def.StatDelta;
		Result.Add(Roll);
	}
	return Result;
}

FCLItemInstance UCLLootRulesService::MakeWeapon(ECLItemRarity Rarity, ECLWeaponSlot SlotFilter, const FString& TableId) const
{
	TArray<const FCLWeaponClassDef*> Candidates;
	for (const FCLWeaponClassDef& Def : WeaponClasses)
	{
		if (Def.Slot == SlotFilter)
		{
			Candidates.Add(&Def);
		}
	}
	if (Candidates.Num() == 0)
	{
		for (const FCLWeaponClassDef& Def : WeaponClasses)
		{
			Candidates.Add(&Def);
		}
	}

	const FCLWeaponClassDef* ClassDef = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	return MakeWeaponOfClass(ClassDef->Id, Rarity, TableId);
}

FCLItemInstance UCLLootRulesService::MakeWeaponOfClass(FName ClassId, ECLItemRarity Rarity, const FString& TableId) const
{
	const FCLWeaponClassDef* ClassDef = FindWeaponClass(ClassId);
	if (!ClassDef && WeaponClasses.Num() > 0)
	{
		ClassDef = &WeaponClasses[0];
	}

	FCLItemInstance Item;
	Item.InstanceId = FGuid::NewGuid();
	Item.Kind = ECLItemKind::Weapon;
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

void UCLLootRulesService::StampBehavior(FCLItemInstance& Item, FName BehaviorId, const FString& DisplayName) const
{
	if (BehaviorId.IsNone())
	{
		return;
	}

	for (const FCLModifierRoll& Existing : Item.Modifiers)
	{
		if (Existing.BehaviorId == BehaviorId)
		{
			return;
		}
	}

	FCLModifierRoll Roll;
	if (const FCLModifierDef* Def = ModifierPool.FindByPredicate(
		[&](const FCLModifierDef& M) { return M.BehaviorId == BehaviorId; }))
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

FCLItemInstance UCLLootRulesService::MakeArmor(ECLItemRarity Rarity, const FString& TableId) const
{
	static const ECLArmorPiece Pieces[] = {
		ECLArmorPiece::Helm, ECLArmorPiece::Arms, ECLArmorPiece::Chest, ECLArmorPiece::Legs
	};

	FCLItemInstance Item;
	Item.InstanceId = FGuid::NewGuid();
	Item.Kind = ECLItemKind::Armor;
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
