#include "Loot/CLLootRulesService.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Containers/Set.h"

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
		if (O->HasField(TEXT("magazine")))
		{
			Stats.Magazine = static_cast<int32>(O->GetNumberField(TEXT("magazine")));
		}
		if (O->HasField(TEXT("rpm")))
		{
			Stats.Rpm = static_cast<float>(O->GetNumberField(TEXT("rpm")));
		}
	}

	FVector ReadVec3(const TArray<TSharedPtr<FJsonValue>>* Arr, const FVector& Fallback)
	{
		if (!Arr || Arr->Num() < 3)
		{
			return Fallback;
		}
		return FVector(
			static_cast<float>((*Arr)[0]->AsNumber()),
			static_cast<float>((*Arr)[1]->AsNumber()),
			static_cast<float>((*Arr)[2]->AsNumber()));
	}

	FRotator ReadRot3(const TArray<TSharedPtr<FJsonValue>>* Arr)
	{
		if (!Arr || Arr->Num() < 3)
		{
			return FRotator::ZeroRotator;
		}
		return FRotator(
			static_cast<float>((*Arr)[0]->AsNumber()),
			static_cast<float>((*Arr)[1]->AsNumber()),
			static_cast<float>((*Arr)[2]->AsNumber()));
	}

	void ScaleWeaponStats(FCLWeaponStats& Stats, float Scale)
	{
		if (FMath::IsNearlyEqual(Scale, 1.f))
		{
			return;
		}
		auto S = [Scale](float& V)
		{
			V *= Scale;
		};
		S(Stats.Impact);
		S(Stats.Range);
		S(Stats.Stability);
		S(Stats.Handling);
		S(Stats.Reload);
		S(Stats.FlinchResist);
		S(Stats.AdsSpeed);
		S(Stats.MobilityBonus);
		S(Stats.Grip);
		S(Stats.Compensator);
	}

	FCLDropSource ReadDropSource(const TSharedPtr<FJsonObject>& O)
	{
		FCLDropSource Src;
		FString Kind;
		if (O->TryGetStringField(TEXT("kind"), Kind))
		{
			if (Kind.Equals(TEXT("raid_boss"), ESearchCase::IgnoreCase)) Src.Kind = ECLDropSourceKind::RaidBoss;
			else if (Kind.Equals(TEXT("raid_mob"), ESearchCase::IgnoreCase)) Src.Kind = ECLDropSourceKind::RaidMob;
			else if (Kind.Equals(TEXT("pvp_award"), ESearchCase::IgnoreCase)) Src.Kind = ECLDropSourceKind::PvpAward;
			else if (Kind.Equals(TEXT("pvp_complete"), ESearchCase::IgnoreCase)) Src.Kind = ECLDropSourceKind::PvpComplete;
			else if (Kind.Equals(TEXT("faction_vendor"), ESearchCase::IgnoreCase)) Src.Kind = ECLDropSourceKind::FactionVendor;
			else Src.Kind = ECLDropSourceKind::World;
		}
		if (O->HasField(TEXT("activityId")))
		{
			Src.ActivityId = FName(*O->GetStringField(TEXT("activityId")));
		}
		O->TryGetStringField(TEXT("activityName"), Src.ActivityName);
		if (O->HasField(TEXT("encounterId")))
		{
			Src.EncounterId = FName(*O->GetStringField(TEXT("encounterId")));
		}
		O->TryGetStringField(TEXT("encounterName"), Src.EncounterName);
		if (O->HasField(TEXT("nodeId")))
		{
			Src.NodeId = FName(*O->GetStringField(TEXT("nodeId")));
		}
		O->TryGetStringField(TEXT("nodeName"), Src.NodeName);
		return Src;
	}

	FCLDropVariant ReadDropVariant(const TSharedPtr<FJsonObject>& O)
	{
		FCLDropVariant V;
		if (O->HasField(TEXT("id")))
		{
			V.Id = FName(*O->GetStringField(TEXT("id")));
		}
		if (O->HasField(TEXT("statBandScale")))
		{
			V.StatBandScale = static_cast<float>(O->GetNumberField(TEXT("statBandScale")));
		}
		O->TryGetStringField(TEXT("thumb"), V.Thumb);
		O->TryGetStringField(TEXT("concept"), V.Concept);
		if (O->HasField(TEXT("branded")))
		{
			V.bBranded = O->GetBoolField(TEXT("branded"));
		}
		else if (V.Id == FName(TEXT("world")) || V.Id == FName(TEXT("plain")))
		{
			V.bBranded = false;
		}
		return V;
	}
}

bool UCLLootRulesService::LoadConfigs()
{
	WeaponClasses.Reset();
	WeaponMakers.Reset();
	WeaponCalibers.Reset();
	WeaponFrames.Reset();
	WeaponParts.Reset();
	WeaponMakes.Reset();
	Sights.Reset();
	ModifierPool.Reset();
	DropTables.Reset();
	Factions.Reset();
	MakeSources.Reset();

	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxModifierStatDelta"), MaxStatDelta, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxModifierBehaviorScale"), MaxBehaviorScale, GGameIni);

	const bool bWeapons = LoadWeaponClasses(ResolveProjectConfigPath(TEXT("Loot/WeaponClasses.json")));
	const bool bMakers = LoadWeaponMakers(ResolveProjectConfigPath(TEXT("Loot/WeaponMakers.json")));
	const bool bCalibers = LoadWeaponCalibers(ResolveProjectConfigPath(TEXT("Loot/WeaponCalibers.json")));
	const bool bFrames = LoadWeaponFrames(ResolveProjectConfigPath(TEXT("Loot/WeaponFrames.json")));
	const bool bParts = LoadWeaponParts(ResolveProjectConfigPath(TEXT("Loot/WeaponParts.json")));
	const bool bMakes = LoadWeaponMakes(ResolveProjectConfigPath(TEXT("Loot/WeaponMakes.json")));
	const bool bSights = LoadSights(ResolveProjectConfigPath(TEXT("Loot/Sights.json")));
	const bool bMods = LoadModifierPool(ResolveProjectConfigPath(TEXT("Loot/ModifierPool.json")));
	const bool bTables = LoadDropTables(ResolveProjectConfigPath(TEXT("Loot/DropTables.json")));
	const bool bFactions = LoadFactions(ResolveProjectConfigPath(TEXT("Factions/Factions.json")));
	if (!(bWeapons && bMakers && bCalibers && bFrames && bParts && bMakes && bSights && bMods && bTables && bFactions))
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("loot_config"),
			TEXT("Loot JSON failed to load")));
		return false;
	}
	BuildMakeSourceIndex();
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
		Def.BandId = Def.Id;
		WeaponClasses.Add(Def);
	}
	return WeaponClasses.Num() > 0;
}

bool UCLLootRulesService::LoadWeaponMakers(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing WeaponMakers.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weaponMakers"), Arr) || !Arr)
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
		FCLWeaponMakerDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->GetStringField(TEXT("displayName"));
		if (O->HasField(TEXT("inspiredBy")))
		{
			Def.InspiredBy = O->GetStringField(TEXT("inspiredBy"));
		}
		WeaponMakers.Add(Def);
	}
	return WeaponMakers.Num() > 0;
}

bool UCLLootRulesService::LoadWeaponCalibers(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing WeaponCalibers.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weaponCalibers"), Arr) || !Arr)
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
		FCLWeaponCaliberDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		if (O->HasField(TEXT("band")))
		{
			Def.Band = FName(*O->GetStringField(TEXT("band")));
		}
		Def.AmmoGrains = O->HasField(TEXT("ammoGrains")) ? static_cast<float>(O->GetNumberField(TEXT("ammoGrains"))) : 0.f;
		Def.MuzzleVelocityMps = O->HasField(TEXT("muzzleVelocityMps"))
			? static_cast<float>(O->GetNumberField(TEXT("muzzleVelocityMps"))) : 0.f;
		WeaponCalibers.Add(Def);
	}
	return WeaponCalibers.Num() > 0;
}

bool UCLLootRulesService::LoadWeaponFrames(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing WeaponFrames.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weaponFrames"), Arr) || !Arr)
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
		FCLWeaponFrameDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.ClassId = FName(*O->GetStringField(TEXT("classId")));
		const TArray<TSharedPtr<FJsonValue>>* Muzzle = nullptr;
		if (O->TryGetArrayField(TEXT("muzzle"), Muzzle))
		{
			Def.Muzzle = ReadVec3(Muzzle, FVector::ZeroVector);
		}
		const TArray<TSharedPtr<FJsonValue>>* Ejector = nullptr;
		if (O->TryGetArrayField(TEXT("ejector"), Ejector))
		{
			Def.Ejector = ReadVec3(Ejector, FVector::ZeroVector);
		}
		const TArray<TSharedPtr<FJsonValue>>* Visuals = nullptr;
		if (O->TryGetArrayField(TEXT("visuals"), Visuals) && Visuals)
		{
			for (const TSharedPtr<FJsonValue>& PV : *Visuals)
			{
				const TSharedPtr<FJsonObject> P = PV->AsObject();
				if (!P.IsValid())
				{
					continue;
				}
				FCLWeaponSocketDef Sock;
				if (P->HasField(TEXT("socket")))
				{
					Sock.Socket = FName(*P->GetStringField(TEXT("socket")));
				}
				if (P->HasField(TEXT("mesh")))
				{
					Sock.Mesh = FName(*P->GetStringField(TEXT("mesh")));
				}
				const TArray<TSharedPtr<FJsonValue>>* Loc = nullptr;
				if (P->TryGetArrayField(TEXT("loc"), Loc))
				{
					Sock.Loc = ReadVec3(Loc, FVector::ZeroVector);
				}
				const TArray<TSharedPtr<FJsonValue>>* Scale = nullptr;
				if (P->TryGetArrayField(TEXT("scale"), Scale))
				{
					Sock.Scale = ReadVec3(Scale, FVector(0.1f));
				}
				const TArray<TSharedPtr<FJsonValue>>* Rot = nullptr;
				if (P->TryGetArrayField(TEXT("rot"), Rot))
				{
					Sock.Rot = ReadRot3(Rot);
				}
				Def.Visuals.Add(Sock);
			}
		}
		WeaponFrames.Add(Def);
	}
	return WeaponFrames.Num() > 0;
}

bool UCLLootRulesService::LoadWeaponParts(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing WeaponParts.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("weaponParts"), Arr) || !Arr)
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
		FCLWeaponPartDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->HasField(TEXT("displayName")) ? O->GetStringField(TEXT("displayName")) : Def.Id.ToString();
		if (O->HasField(TEXT("slot")))
		{
			Def.Slot = FName(*O->GetStringField(TEXT("slot")));
		}
		const TArray<TSharedPtr<FJsonValue>>* Classes = nullptr;
		if (O->TryGetArrayField(TEXT("allowedClassIds"), Classes) && Classes)
		{
			for (const TSharedPtr<FJsonValue>& CV : *Classes)
			{
				Def.AllowedClassIds.Add(FName(*CV->AsString()));
			}
		}
		WeaponParts.Add(Def);
	}
	return WeaponParts.Num() > 0;
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

	TSet<FString> HouseClassCaliber;
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid())
		{
			continue;
		}
		FCLWeaponMakeDef Make;
		Make.Id = FName(*O->GetStringField(TEXT("id")));
		if (O->HasField(TEXT("makerId")))
		{
			Make.MakerId = FName(*O->GetStringField(TEXT("makerId")));
		}
		Make.ClassId = CanonicalWeaponClassId(FName(*O->GetStringField(TEXT("classId"))));
		if (O->HasField(TEXT("frameId")))
		{
			Make.FrameId = FName(*O->GetStringField(TEXT("frameId")));
		}
		if (O->HasField(TEXT("caliberId")))
		{
			Make.CaliberId = FName(*O->GetStringField(TEXT("caliberId")));
		}
		Make.DisplayName = O->GetStringField(TEXT("displayName"));
		if (O->HasField(TEXT("inspiredBy")))
		{
			Make.InspiredBy = O->GetStringField(TEXT("inspiredBy"));
		}
		if (O->HasField(TEXT("stock")))
		{
			Make.Stock = StockFromString(O->GetStringField(TEXT("stock")));
			Make.bHasStock = true;
		}
		Make.bHasMagazine = O->HasField(TEXT("magazine"));
		Make.bHasRpm = O->HasField(TEXT("rpm"));
		FString FireModeName;
		if (O->TryGetStringField(TEXT("fireMode"), FireModeName) && FireModeFromString(FireModeName, Make.FireMode))
		{
			Make.bHasFireMode = true;
		}
		if (O->HasField(TEXT("burstCount")))
		{
			Make.BurstCount = static_cast<int32>(O->GetNumberField(TEXT("burstCount")));
		}
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		if (O->TryGetArrayField(TEXT("allowedPartSlots"), Slots) && Slots)
		{
			for (const TSharedPtr<FJsonValue>& SV : *Slots)
			{
				const FName SlotId(*SV->AsString());
				if (SlotId == FName(TEXT("underbarrel_grenade")))
				{
					continue;
				}
				Make.AllowedPartSlots.Add(SlotId);
			}
		}
		ReadPhysicalStats(O, Make.Stats);

		const FString Key = FString::Printf(TEXT("%s|%s|%s"),
			*Make.MakerId.ToString(), *Make.ClassId.ToString(), *Make.CaliberId.ToString());
		if (HouseClassCaliber.Contains(Key))
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: duplicate house×class×caliber make %s (%s)"), *Make.Id.ToString(), *Key);
		}
		HouseClassCaliber.Add(Key);
		if (O->HasField(TEXT("thumb")))
		{
			Make.Thumb = O->GetStringField(TEXT("thumb"));
		}
		if (O->HasField(TEXT("concept")))
		{
			Make.Concept = O->GetStringField(TEXT("concept"));
		}
		if (O->HasField(TEXT("worldDrop")))
		{
			Make.bWorldDrop = O->GetBoolField(TEXT("worldDrop"));
		}
		if (O->HasField(TEXT("worldOnly")))
		{
			Make.bWorldOnly = O->GetBoolField(TEXT("worldOnly"));
		}
		if (O->HasField(TEXT("primarySourceId")))
		{
			Make.PrimarySourceId = FName(*O->GetStringField(TEXT("primarySourceId")));
		}
		if (O->HasField(TEXT("factionId")))
		{
			Make.FactionId = FName(*O->GetStringField(TEXT("factionId")));
		}
		WeaponMakes.Add(Make);
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
		if (O->HasField(TEXT("source")))
		{
			if (const TSharedPtr<FJsonObject> SrcObj = O->GetObjectField(TEXT("source")))
			{
				Table.Source = ReadDropSource(SrcObj);
				Table.bHasSource = true;
			}
		}
		if (O->HasField(TEXT("variant")))
		{
			if (const TSharedPtr<FJsonObject> VarObj = O->GetObjectField(TEXT("variant")))
			{
				Table.Variant = ReadDropVariant(VarObj);
			}
		}
		if (O->HasField(TEXT("factionId")))
		{
			Table.FactionId = FName(*O->GetStringField(TEXT("factionId")));
		}
		const TArray<TSharedPtr<FJsonValue>>* Rolls = nullptr;
		if (O->TryGetArrayField(TEXT("rolls"), Rolls) && Rolls)
		{
			for (const TSharedPtr<FJsonValue>& RV : *Rolls)
			{
				const TSharedPtr<FJsonObject> R = RV->AsObject();
				if (!R.IsValid()) continue;
				FCLDropRoll Roll;
				Roll.ItemKind = ItemKindFromString(R->GetStringField(TEXT("itemKind")));
				if (R->HasField(TEXT("rarity")))
				{
					Roll.Rarity = RarityFromString(R->GetStringField(TEXT("rarity")));
				}
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
				if (R->HasField(TEXT("makeId")))
				{
					Roll.MakeId = FName(*R->GetStringField(TEXT("makeId")));
				}
				if (R->HasField(TEXT("pool")))
				{
					Roll.Pool = FName(*R->GetStringField(TEXT("pool")));
					if (Roll.Pool == FName(TEXT("world")) && !Table.bHasSource)
					{
						Table.Source.Kind = ECLDropSourceKind::World;
						Table.Source.ActivityName = TEXT("World drop");
						Table.Source.NodeName = TEXT("Any activity");
						Table.bHasSource = true;
					}
				}
				Table.Rolls.Add(Roll);
			}
		}
		DropTables.Add(Table);
	}
	return DropTables.Num() > 0;
}

bool UCLLootRulesService::LoadFactions(const FString& Path)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: missing Factions.json at %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("factions"), Arr) || !Arr)
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
		FCLFactionDef Def;
		Def.Id = FName(*O->GetStringField(TEXT("id")));
		Def.DisplayName = O->HasField(TEXT("displayName")) ? O->GetStringField(TEXT("displayName")) : Def.Id.ToString();
		if (O->HasField(TEXT("kind")))
		{
			Def.Kind = FName(*O->GetStringField(TEXT("kind")));
		}
		if (O->HasField(TEXT("vendorTableId")))
		{
			Def.VendorTableId = FName(*O->GetStringField(TEXT("vendorTableId")));
		}
		const TArray<TSharedPtr<FJsonValue>>* Slots = nullptr;
		if (O->TryGetArrayField(TEXT("uniformSlots"), Slots) && Slots)
		{
			for (const TSharedPtr<FJsonValue>& SV : *Slots)
			{
				Def.UniformSlots.Add(FName(*SV->AsString()));
			}
		}
		Factions.Add(Def);
	}
	return Factions.Num() > 0;
}

void UCLLootRulesService::BuildMakeSourceIndex()
{
	MakeSources.Reset();
	FName WorldTableId = NAME_None;
	int32 WorldWeaponWeight = 0;
	int32 WorldTableTotal = 0;
	FCLDropVariant WorldVariant;
	WorldVariant.Id = FName(TEXT("world"));
	WorldVariant.StatBandScale = 0.7f;
	WorldVariant.bBranded = false;
	FCLDropSource WorldSource;
	WorldSource.Kind = ECLDropSourceKind::World;
	WorldSource.ActivityName = TEXT("World drop");
	WorldSource.NodeName = TEXT("Any activity");

	for (const FCLDropTable& Table : DropTables)
	{
		int32 Total = 0;
		for (const FCLDropRoll& R : Table.Rolls)
		{
			Total += FMath::Max(1, R.Weight);
		}
		for (const FCLDropRoll& R : Table.Rolls)
		{
			if (R.ItemKind != ECLItemKind::Weapon)
			{
				continue;
			}
			if (R.Pool == FName(TEXT("world")))
			{
				WorldTableId = Table.Id;
				WorldWeaponWeight = FMath::Max(1, R.Weight);
				WorldTableTotal = Total;
				if (Table.Variant.StatBandScale > 0.f)
				{
					WorldVariant = Table.Variant;
					WorldVariant.bBranded = false;
				}
				if (Table.bHasSource)
				{
					WorldSource = Table.Source;
					WorldSource.Kind = ECLDropSourceKind::World;
				}
				continue;
			}
			if (R.MakeId.IsNone())
			{
				continue;
			}
			FCLWeaponSourceRef Ref;
			Ref.TableId = Table.Id;
			Ref.MakeId = R.MakeId;
			Ref.Source = Table.Source;
			Ref.Variant = Table.Variant;
			Ref.WeaponWeight = FMath::Max(1, R.Weight);
			Ref.TableWeightTotal = FMath::Max(1, Total);
			Ref.FactionId = Table.FactionId;
			if (const FCLWeaponMakeDef* Make = FindWeaponMake(R.MakeId))
			{
				if (Ref.FactionId.IsNone())
				{
					Ref.FactionId = Make->FactionId;
				}
			}
			MakeSources.FindOrAdd(R.MakeId).Add(Ref);
		}
	}

	for (FCLWeaponMakeDef& Make : WeaponMakes)
	{
		TArray<FCLWeaponSourceRef>& Srcs = MakeSources.FindOrAdd(Make.Id);
		bool bHasPrestige = false;
		bool bHasWorld = false;
		for (const FCLWeaponSourceRef& S : Srcs)
		{
			if (S.Source.Kind == ECLDropSourceKind::World)
			{
				bHasWorld = true;
			}
			else
			{
				bHasPrestige = true;
			}
		}
		if (Make.bWorldDrop && !bHasWorld && WorldTableId != NAME_None)
		{
			FCLWeaponSourceRef Ref;
			Ref.TableId = WorldTableId;
			Ref.MakeId = Make.Id;
			Ref.Source = WorldSource;
			Ref.Variant = WorldVariant;
			Ref.WeaponWeight = WorldWeaponWeight;
			Ref.TableWeightTotal = FMath::Max(1, WorldTableTotal);
			Srcs.Add(Ref);
			bHasWorld = true;
		}
		Make.bWorldOnly = Make.bWorldDrop && !bHasPrestige;
		if (Make.PrimarySourceId.IsNone())
		{
			FName PrestigeId = NAME_None;
			FName WorldId = NAME_None;
			for (const FCLWeaponSourceRef& S : Srcs)
			{
				if (S.Source.Kind == ECLDropSourceKind::World)
				{
					WorldId = S.TableId;
				}
				else if (PrestigeId.IsNone())
				{
					PrestigeId = S.TableId;
				}
			}
			Make.PrimarySourceId = PrestigeId.IsNone() ? WorldId : PrestigeId;
		}
	}
}

const FCLFactionDef* UCLLootRulesService::FindFaction(FName Id) const
{
	return Factions.FindByPredicate([&](const FCLFactionDef& D) { return D.Id == Id; });
}

const FCLDropTable* UCLLootRulesService::FindDropTable(FName Id) const
{
	return DropTables.FindByPredicate([&](const FCLDropTable& T) { return T.Id == Id; });
}

TArray<FCLWeaponMakeDef> UCLLootRulesService::MakesMatching(FName ClassId, FName MakerId) const
{
	TArray<FCLWeaponMakeDef> Out;
	const FName Canonical = ClassId.IsNone() ? NAME_None : CanonicalWeaponClassId(ClassId);
	for (const FCLWeaponMakeDef& Make : WeaponMakes)
	{
		if (!Canonical.IsNone() && Make.ClassId != Canonical)
		{
			continue;
		}
		if (!MakerId.IsNone() && Make.MakerId != MakerId)
		{
			continue;
		}
		Out.Add(Make);
	}
	return Out;
}

TArray<FCLWeaponSourceRef> UCLLootRulesService::SourcesForMake(FName MakeId) const
{
	if (const TArray<FCLWeaponSourceRef>* Found = MakeSources.Find(MakeId))
	{
		return *Found;
	}
	return TArray<FCLWeaponSourceRef>();
}

FCLWeaponSourceRef UCLLootRulesService::PrimarySourceForMake(FName MakeId) const
{
	const TArray<FCLWeaponSourceRef> Srcs = SourcesForMake(MakeId);
	if (const FCLWeaponMakeDef* Make = FindWeaponMake(MakeId))
	{
		if (!Make->PrimarySourceId.IsNone())
		{
			for (const FCLWeaponSourceRef& S : Srcs)
			{
				if (S.TableId == Make->PrimarySourceId)
				{
					return S;
				}
			}
		}
	}
	return Srcs.Num() > 0 ? Srcs[0] : FCLWeaponSourceRef();
}

void UCLLootRulesService::StatBandFor(FName MakeId, const FCLWeaponSourceRef& Source, FCLWeaponStats& OutMin, FCLWeaponStats& OutMax) const
{
	FCLWeaponClassDef Composed;
	if (!ComposeEquippedClass(MakeId, Composed))
	{
		OutMin = FCLWeaponStats();
		OutMax = FCLWeaponStats();
		return;
	}
	const float Band = Source.Variant.StatBandScale > 0.f ? Source.Variant.StatBandScale : 1.f;
	const float Delta = MaxStatDelta * Band;
	OutMin = Composed.BaseStats;
	OutMax = Composed.BaseStats;
	auto Spread = [Delta](float Base, float& MinV, float& MaxV, float Lo, float Hi)
	{
		MinV = FMath::Clamp(Base - Delta, Lo, Hi);
		MaxV = FMath::Clamp(Base + Delta, Lo, Hi);
	};
	Spread(Composed.BaseStats.Range, OutMin.Range, OutMax.Range, 0.f, 1.5f);
	Spread(Composed.BaseStats.Stability, OutMin.Stability, OutMax.Stability, 0.f, 1.5f);
	Spread(Composed.BaseStats.Handling, OutMin.Handling, OutMax.Handling, 0.f, 1.5f);
	Spread(Composed.BaseStats.Reload, OutMin.Reload, OutMax.Reload, 0.f, 1.5f);
	Spread(Composed.BaseStats.Grip, OutMin.Grip, OutMax.Grip, 0.05f, 1.f);
	OutMin.MassKg = FMath::Max(0.25f, Composed.BaseStats.MassKg * (1.f - 0.06f * Band));
	OutMax.MassKg = Composed.BaseStats.MassKg * (1.f + 0.06f * Band);
	OutMin.BarrelLengthCm = FMath::Max(6.f, Composed.BaseStats.BarrelLengthCm * (1.f - 0.04f * Band));
	OutMax.BarrelLengthCm = Composed.BaseStats.BarrelLengthCm * (1.f + 0.04f * Band);
}

TArray<FCLModifierDef> UCLLootRulesService::ModsForMake(FName MakeId) const
{
	TArray<FCLModifierDef> Out;
	const FCLWeaponMakeDef* Make = FindWeaponMake(MakeId);
	const FCLWeaponClassDef* Band = Make ? FindWeaponClass(Make->ClassId) : nullptr;
	for (const FCLModifierDef& M : ModifierPool)
	{
		if (M.BehaviorId == FName(TEXT("prox_detonate")))
		{
			if (!Band || Band->Fire.Mode != ECLWeaponFireMode::Grenade)
			{
				continue;
			}
		}
		Out.Add(M);
	}
	return Out;
}

TArray<FCLWeaponPartDef> UCLLootRulesService::PartsForMake(FName MakeId) const
{
	TArray<FCLWeaponPartDef> Out;
	const FCLWeaponMakeDef* Make = FindWeaponMake(MakeId);
	if (!Make)
	{
		return Out;
	}
	for (const FCLWeaponPartDef& Part : WeaponParts)
	{
		if (!Make->AllowedPartSlots.Contains(Part.Slot))
		{
			continue;
		}
		if (Part.AllowedClassIds.Num() > 0 && !Part.AllowedClassIds.Contains(Make->ClassId))
		{
			continue;
		}
		Out.Add(Part);
	}
	return Out;
}

ECLDropSourceKind UCLLootRulesService::SourceKindFromString(const FString& S)
{
	if (S.Equals(TEXT("raid_boss"), ESearchCase::IgnoreCase)) return ECLDropSourceKind::RaidBoss;
	if (S.Equals(TEXT("raid_mob"), ESearchCase::IgnoreCase)) return ECLDropSourceKind::RaidMob;
	if (S.Equals(TEXT("pvp_award"), ESearchCase::IgnoreCase)) return ECLDropSourceKind::PvpAward;
	if (S.Equals(TEXT("pvp_complete"), ESearchCase::IgnoreCase)) return ECLDropSourceKind::PvpComplete;
	if (S.Equals(TEXT("faction_vendor"), ESearchCase::IgnoreCase)) return ECLDropSourceKind::FactionVendor;
	return ECLDropSourceKind::World;
}

ECLItemKind UCLLootRulesService::ItemKindFromString(const FString& S)
{
	if (S.Equals(TEXT("armor"), ESearchCase::IgnoreCase)) return ECLItemKind::Armor;
	if (S.Equals(TEXT("empty"), ESearchCase::IgnoreCase) || S.Equals(TEXT("miss"), ESearchCase::IgnoreCase))
	{
		return ECLItemKind::Empty;
	}
	return ECLItemKind::Weapon;
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
	const FName Canonical = CanonicalWeaponClassId(Id);
	return WeaponClasses.FindByPredicate([&](const FCLWeaponClassDef& D) { return D.Id == Canonical; });
}

const FCLWeaponMakeDef* UCLLootRulesService::FindWeaponMake(FName Id) const
{
	return WeaponMakes.FindByPredicate([&](const FCLWeaponMakeDef& D) { return D.Id == Id; });
}

const FCLWeaponMakerDef* UCLLootRulesService::FindWeaponMaker(FName Id) const
{
	return WeaponMakers.FindByPredicate([&](const FCLWeaponMakerDef& D) { return D.Id == Id; });
}

const FCLWeaponCaliberDef* UCLLootRulesService::FindWeaponCaliber(FName Id) const
{
	return WeaponCalibers.FindByPredicate([&](const FCLWeaponCaliberDef& D) { return D.Id == Id; });
}

const FCLWeaponFrameDef* UCLLootRulesService::FindWeaponFrame(FName Id) const
{
	return WeaponFrames.FindByPredicate([&](const FCLWeaponFrameDef& D) { return D.Id == Id; });
}

const FCLWeaponFrameDef* UCLLootRulesService::FindWeaponFrameForClass(FName ClassId) const
{
	const FName Canonical = CanonicalWeaponClassId(ClassId);
	if (const FCLWeaponFrameDef* Named = FindWeaponFrame(FName(*FString::Printf(TEXT("%s_frame"), *Canonical.ToString()))))
	{
		return Named;
	}
	return WeaponFrames.FindByPredicate([&](const FCLWeaponFrameDef& D) { return D.ClassId == Canonical; });
}

FName UCLLootRulesService::CanonicalWeaponClassId(FName Id)
{
	if (Id == FName(TEXT("sidearm")) || Id == FName(TEXT("hand_cannon")))
	{
		return FName(TEXT("pistol"));
	}
	if (Id == FName(TEXT("auto_rifle")) || Id == FName(TEXT("pulse_rifle")))
	{
		return FName(TEXT("rifle"));
	}
	if (Id == FName(TEXT("scout_rifle")))
	{
		return FName(TEXT("dmr"));
	}
	return Id;
}

void UCLLootRulesService::ApplyMakeOverlay(FCLWeaponClassDef& ClassDef, const FCLWeaponMakeDef& Make) const
{
	ClassDef.BandId = ClassDef.Id;
	ClassDef.Id = Make.Id;
	ClassDef.DisplayName = Make.DisplayName;
	if (Make.bHasStock)
	{
		ClassDef.Stock = Make.Stock;
	}
	auto Overlay = [](float Src, float& Dst)
	{
		if (Src > 0.f)
		{
			Dst = Src;
		}
	};
	Overlay(Make.Stats.MassKg, ClassDef.BaseStats.MassKg);
	Overlay(Make.Stats.BarrelLengthCm, ClassDef.BaseStats.BarrelLengthCm);
	Overlay(Make.Stats.AmmoGrains, ClassDef.BaseStats.AmmoGrains);
	Overlay(Make.Stats.MuzzleVelocityMps, ClassDef.BaseStats.MuzzleVelocityMps);
	if (Make.Stats.Grip > 0.f)
	{
		ClassDef.BaseStats.Grip = Make.Stats.Grip;
	}
	ClassDef.BaseStats.Compensator = Make.Stats.Compensator;
	Overlay(Make.Stats.DrawSeconds, ClassDef.BaseStats.DrawSeconds);
	Overlay(Make.Stats.StowSeconds, ClassDef.BaseStats.StowSeconds);
	if (Make.bHasMagazine)
	{
		ClassDef.BaseStats.Magazine = Make.Stats.Magazine;
	}
	if (Make.bHasRpm)
	{
		ClassDef.BaseStats.Rpm = Make.Stats.Rpm;
	}
	if (Make.bHasFireMode)
	{
		ClassDef.Fire.Mode = Make.FireMode;
		if (Make.BurstCount > 1)
		{
			ClassDef.Fire.BurstCount = Make.BurstCount;
		}
	}
	else if (Make.BurstCount > 1)
	{
		ClassDef.Fire.BurstCount = Make.BurstCount;
		ClassDef.Fire.Mode = ECLWeaponFireMode::Burst;
	}
	if (const FCLWeaponCaliberDef* Cal = FindWeaponCaliber(Make.CaliberId))
	{
		if (Make.Stats.AmmoGrains <= 0.f && Cal->AmmoGrains > 0.f)
		{
			ClassDef.BaseStats.AmmoGrains = Cal->AmmoGrains;
		}
		if (Make.Stats.MuzzleVelocityMps <= 0.f && Cal->MuzzleVelocityMps > 0.f)
		{
			ClassDef.BaseStats.MuzzleVelocityMps = Cal->MuzzleVelocityMps;
		}
	}
}

const FCLWeaponMakeDef* UCLLootRulesService::PickMakeForClass(FName ClassId) const
{
	const FName Canonical = CanonicalWeaponClassId(ClassId);
	TArray<const FCLWeaponMakeDef*> Matches;
	for (const FCLWeaponMakeDef& Make : WeaponMakes)
	{
		if (Make.ClassId == Canonical)
		{
			Matches.Add(&Make);
		}
	}
	if (Matches.Num() == 0)
	{
		return nullptr;
	}
	return Matches[FMath::RandRange(0, Matches.Num() - 1)];
}

const FCLWeaponMakeDef* UCLLootRulesService::PickWorldMake() const
{
	TArray<const FCLWeaponMakeDef*> Matches;
	for (const FCLWeaponMakeDef& Make : WeaponMakes)
	{
		if (Make.bWorldDrop)
		{
			Matches.Add(&Make);
		}
	}
	if (Matches.Num() == 0)
	{
		return nullptr;
	}
	return Matches[FMath::RandRange(0, Matches.Num() - 1)];
}

bool UCLLootRulesService::ComposeEquippedClass(FName DefinitionId, FCLWeaponClassDef& Out) const
{
	if (const FCLWeaponMakeDef* Make = FindWeaponMake(DefinitionId))
	{
		const FCLWeaponClassDef* Band = FindWeaponClass(Make->ClassId);
		if (!Band)
		{
			return false;
		}
		Out = *Band;
		ApplyMakeOverlay(Out, *Make);
		return true;
	}

	const FName Canonical = CanonicalWeaponClassId(DefinitionId);
	if (const FCLWeaponMakeDef* Picked = PickMakeForClass(Canonical))
	{
		const FCLWeaponClassDef* Band = FindWeaponClass(Picked->ClassId);
		if (!Band)
		{
			return false;
		}
		Out = *Band;
		ApplyMakeOverlay(Out, *Picked);
		return true;
	}

	if (const FCLWeaponClassDef* Band = FindWeaponClass(Canonical))
	{
		Out = *Band;
		return true;
	}
	return false;
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

	if (Chosen->ItemKind == ECLItemKind::Empty)
	{
		return false;
	}

	const float BandScale = Table->Variant.StatBandScale > 0.f ? Table->Variant.StatBandScale : 1.f;
	if (Chosen->ItemKind == ECLItemKind::Armor)
	{
		OutItem = MakeArmor(Chosen->Rarity, TableId.ToString());
	}
	else if (!Chosen->MakeId.IsNone())
	{
		if (const FCLWeaponMakeDef* Make = FindWeaponMake(Chosen->MakeId))
		{
			OutItem = StampWeaponFromMake(*Make, Chosen->Rarity, TableId.ToString(), BandScale);
		}
		else
		{
			return false;
		}
	}
	else if (Chosen->Pool == FName(TEXT("world")))
	{
		if (const FCLWeaponMakeDef* Make = PickWorldMake())
		{
			OutItem = StampWeaponFromMake(*Make, Chosen->Rarity, TableId.ToString(), BandScale);
		}
		else
		{
			return false;
		}
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

TArray<FCLModifierRoll> UCLLootRulesService::RollModifiers(ECLItemRarity Rarity, float StatBandScale) const
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
		Roll.BehaviorScale = FMath::Clamp(Def.BehaviorScale * StatBandScale, 0.f, MaxBehaviorScale);
		Roll.StatDelta = Def.StatDelta;
		ScaleWeaponStats(Roll.StatDelta, StatBandScale);
		Result.Add(Roll);
	}
	return Result;
}

FCLItemInstance UCLLootRulesService::MakeWeapon(ECLItemRarity Rarity, ECLWeaponSlot SlotFilter, const FString& TableId) const
{
	TArray<const FCLWeaponMakeDef*> Candidates;
	for (const FCLWeaponMakeDef& Make : WeaponMakes)
	{
		const FCLWeaponClassDef* Band = FindWeaponClass(Make.ClassId);
		if (Band && Band->Slot == SlotFilter)
		{
			Candidates.Add(&Make);
		}
	}
	if (Candidates.Num() == 0)
	{
		for (const FCLWeaponMakeDef& Make : WeaponMakes)
		{
			Candidates.Add(&Make);
		}
	}
	if (Candidates.Num() == 0)
	{
		return FCLItemInstance();
	}
	return StampWeaponFromMake(*Candidates[FMath::RandRange(0, Candidates.Num() - 1)], Rarity, TableId);
}

FCLItemInstance UCLLootRulesService::MakeWeaponOfClass(FName ClassId, ECLItemRarity Rarity, const FString& TableId) const
{
	if (const FCLWeaponMakeDef* Exact = FindWeaponMake(ClassId))
	{
		return StampWeaponFromMake(*Exact, Rarity, TableId);
	}
	if (const FCLWeaponMakeDef* Picked = PickMakeForClass(ClassId))
	{
		return StampWeaponFromMake(*Picked, Rarity, TableId);
	}
	FCLItemInstance Empty;
	Empty.DefinitionId = ClassId;
	Empty.DisplayName = ClassId.ToString();
	Empty.Kind = ECLItemKind::Weapon;
	Empty.Rarity = Rarity;
	return Empty;
}

FCLItemInstance UCLLootRulesService::StampWeaponFromMake(const FCLWeaponMakeDef& Make, ECLItemRarity Rarity, const FString& TableId, float StatBandScale) const
{
	FCLWeaponClassDef Composed;
	FCLItemInstance Item;
	Item.InstanceId = FGuid::NewGuid();
	Item.Kind = ECLItemKind::Weapon;
	Item.Rarity = Rarity;
	if (ComposeEquippedClass(Make.Id, Composed))
	{
		Item.DefinitionId = Make.Id;
		Item.DisplayName = Make.DisplayName;
		Item.Weapon.Slot = Composed.Slot;
		Item.BaseStats = Composed.BaseStats;
		Item.SightId = Composed.SightId;
	}
	else
	{
		Item.DefinitionId = Make.Id;
		Item.DisplayName = Make.DisplayName;
	}
	if (!TableId.Equals(TEXT("starter")))
	{
		Item.Modifiers = RollModifiers(Rarity, StatBandScale);
		const float Jitter = 0.08f * StatBandScale;
		Item.BaseStats.Grip = FMath::Clamp(Item.BaseStats.Grip + FMath::FRandRange(-Jitter, Jitter), 0.15f, 1.f);
		Item.BaseStats.MassKg = FMath::Max(0.25f, Item.BaseStats.MassKg * FMath::FRandRange(1.f - 0.06f * StatBandScale, 1.f + 0.06f * StatBandScale));
		Item.BaseStats.BarrelLengthCm = FMath::Max(6.f, Item.BaseStats.BarrelLengthCm * FMath::FRandRange(1.f - 0.04f * StatBandScale, 1.f + 0.04f * StatBandScale));
	}
	Item.SourceTableId = TableId;
	Item.RealmId = FName(TEXT("local"));
	Item.EarnedAt = FDateTime::UtcNow();
	Item.RecomputeFinalStats(MaxStatDelta * StatBandScale);
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
