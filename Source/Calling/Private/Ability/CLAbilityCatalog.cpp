#include "Ability/CLAbilityCatalog.h"
#include "Ability/CLAbility.h"
#include "Ability/CLAbilitySlots.h"
#include "Ability/CLAbilityConcrete.h"
#include "Ability/CLAbilityTypeRegistry.h"
#include "UObject/Class.h"
#include "Player/CLVanguardCharacter.h"
#include "Player/CLPathfinderCharacter.h"
#include "Player/CLWardenCharacter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"

UCLAbilityCatalog* UCLAbilityCatalog::Get(UObject* WorldContext)
{
	static TWeakObjectPtr<UCLAbilityCatalog> Singleton;
	if (UCLAbilityCatalog* Existing = Singleton.Get())
	{
		return Existing;
	}
	UObject* Outer = WorldContext ? WorldContext->GetWorld() : static_cast<UObject*>(GetTransientPackage());
	if (!Outer)
	{
		Outer = GetTransientPackage();
	}
	UCLAbilityCatalog* Created = NewObject<UCLAbilityCatalog>(Outer, NAME_None, RF_Transient);
	if (!Created->LoadFiles())
	{
		UCLErrorBoundary::ReportStatic(WorldContext, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("ability_catalog"),
			TEXT("Ability catalog failed to load")));
	}
	Singleton = Created;
	return Created;
}

FString UCLAbilityCatalog::ClassesDir()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Classes"));
}

ECLClassId UCLAbilityCatalog::ClassIdFromName(const FString& Name)
{
	if (Name.Equals(TEXT("pathfinder"), ESearchCase::IgnoreCase)) return ECLClassId::Pathfinder;
	if (Name.Equals(TEXT("warden"), ESearchCase::IgnoreCase)) return ECLClassId::Warden;
	return ECLClassId::Vanguard;
}

UClass* UCLAbilityCatalog::ExpectedSlotBase(ECLAbilitySlot Slot)
{
	switch (Slot)
	{
	case ECLAbilitySlot::Grenade: return UCLGrenadeAbility::StaticClass();
	case ECLAbilitySlot::Shield: return UCLShieldAbility::StaticClass();
	case ECLAbilitySlot::Evasion: return UCLEvasionAbility::StaticClass();
	case ECLAbilitySlot::Dash: return UCLDashAbility::StaticClass();
	case ECLAbilitySlot::Melee: return UCLMeleeAbility::StaticClass();
	case ECLAbilitySlot::Jump: return UCLJumpAbility::StaticClass();
	case ECLAbilitySlot::Super: return UCLSuperAbility::StaticClass();
	default: return UCLAbility::StaticClass();
	}
}

UClass* UCLAbilityCatalog::FindType(const FString& TypeName) const
{
	if (UClass* Found = FCLAbilityTypeRegistry::Find(TypeName))
	{
		return Found;
	}
	TArray<UClass*> Derived;
	GetDerivedClasses(UCLAbility::StaticClass(), Derived, true);
	for (UClass* Class : Derived)
	{
		if (Class && !Class->HasAnyClassFlags(CLASS_Abstract) && Class->GetName() == TypeName)
		{
			FCLAbilityTypeRegistry::Register(TypeName, Class);
			return Class;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> UCLAbilityCatalog::MergeFields(const TSharedPtr<FJsonObject>& Base, const TSharedPtr<FJsonObject>& Overlay)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	if (Base.IsValid())
	{
		Out->Values = Base->Values;
	}
	if (Overlay.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Overlay->Values)
		{
			Out->Values.Add(Pair);
		}
	}
	return Out;
}

bool UCLAbilityCatalog::LoadFiles()
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLAbilityFeelSettings"), TEXT("AbilityCooldownScale"), CooldownScale, GGameIni);

	Abilities.Reset();
	Classes.Reset();

	FString CatalogText;
	const FString CatalogPath = FPaths::Combine(ClassesDir(), TEXT("AbilityCatalog.json"));
	if (!FFileHelper::LoadFileToString(CatalogText, *CatalogPath))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: missing ability catalog %s"), *CatalogPath);
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("ability_catalog_missing"),
			FString::Printf(TEXT("Missing AbilityCatalog.json at %s"), *CatalogPath)));
		return false;
	}

	TSharedPtr<FJsonObject> CatalogObj;
	const TSharedRef<TJsonReader<>> CatalogReader = TJsonReaderFactory<>::Create(CatalogText);
	if (!FJsonSerializer::Deserialize(CatalogReader, CatalogObj) || !CatalogObj.IsValid())
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: failed to parse AbilityCatalog.json"));
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("ability_catalog_parse"),
			TEXT("Failed to parse AbilityCatalog.json")));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (CatalogObj->TryGetArrayField(TEXT("abilities"), List) && List)
	{
		for (const TSharedPtr<FJsonValue>& Value : *List)
		{
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			if (!Obj.IsValid())
			{
				continue;
			}
			FAbilityDef Def;
			Def.Id = FName(*Obj->GetStringField(TEXT("id")));
			Def.Type = Obj->GetStringField(TEXT("type"));
			if (!CLParseAbilitySlot(Obj->GetStringField(TEXT("slot")), Def.Slot))
			{
				UE_LOG(LogCalling, Warning, TEXT("Calling: ability %s has invalid slot"), *Def.Id.ToString());
				continue;
			}
			Def.Fields = Obj;
			Abilities.Add(Def.Id, Def);
		}
	}

	const TCHAR* ClassFiles[] = { TEXT("Vanguard.json"), TEXT("Pathfinder.json"), TEXT("Warden.json") };
	for (const TCHAR* File : ClassFiles)
	{
		FString Text;
		const FString Path = FPaths::Combine(ClassesDir(), File);
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogCalling, Error, TEXT("Calling: missing class kit %s"), *Path);
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("class_kit_missing"),
				Path));
			continue;
		}
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			UE_LOG(LogCalling, Error, TEXT("Calling: failed to parse class kit %s"), *Path);
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::NonDeterministic, TEXT("class_kit_parse"), Path));
			continue;
		}
		FClassDef Def;
		Def.Id = Obj->GetStringField(TEXT("id"));
		Def.CharacterClass = Obj->GetStringField(TEXT("characterClass"));
		if (const TSharedPtr<FJsonObject> Slots = Obj->GetObjectField(TEXT("slots")))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Slots->Values)
			{
				ECLAbilitySlot Slot;
				if (!CLParseAbilitySlot(Pair.Key, Slot))
				{
					continue;
				}
				const TSharedPtr<FJsonObject> Bind = Pair.Value->AsObject();
				if (!Bind.IsValid())
				{
					continue;
				}
				Def.SlotAbility.Add(Slot, FName(*Bind->GetStringField(TEXT("ability"))));
				if (Bind->HasField(TEXT("args")))
				{
					Def.SlotArgs.Add(Slot, Bind->GetObjectField(TEXT("args")));
				}
			}
		}
		Classes.Add(ClassIdFromName(Def.Id), Def);
	}

	bLoaded = Abilities.Num() > 0 && Classes.Num() > 0;
	return bLoaded;
}

UCLAbility* UCLAbilityCatalog::SpawnBoundAbility(UObject* Outer, ECLClassId ClassId, ECLAbilitySlot Slot) const
{
	const FClassDef* ClassDef = Classes.Find(ClassId);
	if (!ClassDef)
	{
		return nullptr;
	}
	const FName* AbilityId = ClassDef->SlotAbility.Find(Slot);
	if (!AbilityId)
	{
		return nullptr;
	}
	const FAbilityDef* AbilityDef = Abilities.Find(*AbilityId);
	if (!AbilityDef)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: class %s slot missing catalog id %s"), *ClassDef->Id, *AbilityId->ToString());
		return nullptr;
	}
	if (AbilityDef->Slot != Slot)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: %s is a %d ability, cannot bind to slot %d"), *AbilityId->ToString(), static_cast<int32>(AbilityDef->Slot), static_cast<int32>(Slot));
		return nullptr;
	}

	UClass* Type = FindType(AbilityDef->Type);
	UClass* SlotBase = ExpectedSlotBase(Slot);
	if (!Type || !Type->IsChildOf(SlotBase))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: type %s is not valid for slot %d"), *AbilityDef->Type, static_cast<int32>(Slot));
		return nullptr;
	}

	UCLAbility* Inst = NewObject<UCLAbility>(Outer, Type);
	Inst->Id = AbilityDef->Id;
	Inst->Slot = Slot;
	const TSharedPtr<FJsonObject>* Args = ClassDef->SlotArgs.Find(Slot);
	Inst->ApplyTuning(MergeFields(AbilityDef->Fields, Args ? *Args : nullptr), CooldownScale);
	return Inst;
}

FString UCLAbilityCatalog::GetCharacterClassName(ECLClassId ClassId) const
{
	if (const FClassDef* ClassDef = Classes.Find(ClassId))
	{
		return ClassDef->CharacterClass;
	}
	return FString();
}

TSubclassOf<APawn> UCLAbilityCatalog::ResolvePawnClass(ECLClassId ClassId) const
{
	switch (ClassId)
	{
	case ECLClassId::Pathfinder: return ACLPathfinderCharacter::StaticClass();
	case ECLClassId::Warden: return ACLWardenCharacter::StaticClass();
	default: return ACLVanguardCharacter::StaticClass();
	}
}
