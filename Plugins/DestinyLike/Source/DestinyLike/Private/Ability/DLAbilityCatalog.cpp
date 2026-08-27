#include "Ability/DLAbilityCatalog.h"
#include "Ability/DLAbility.h"
#include "Ability/DLAbilitySlots.h"
#include "Ability/DLAbilityConcrete.h"
#include "Ability/DLAbilityTypeRegistry.h"
#include "UObject/Class.h"
#include "Player/DLVanguardCharacter.h"
#include "Player/DLPathfinderCharacter.h"
#include "Player/DLWardenCharacter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/Engine.h"
#include "Core/DLLog.h"
#include "Core/DLError.h"
#include "Game/DLErrorBoundary.h"

UDLAbilityCatalog* UDLAbilityCatalog::Get(UObject* WorldContext)
{
	static TWeakObjectPtr<UDLAbilityCatalog> Singleton;
	if (UDLAbilityCatalog* Existing = Singleton.Get())
	{
		return Existing;
	}
	UObject* Outer = WorldContext ? WorldContext->GetWorld() : static_cast<UObject*>(GetTransientPackage());
	if (!Outer)
	{
		Outer = GetTransientPackage();
	}
	UDLAbilityCatalog* Created = NewObject<UDLAbilityCatalog>(Outer, NAME_None, RF_Transient);
	if (!Created->LoadFiles())
	{
		UDLErrorBoundary::ReportStatic(WorldContext, FDLError::Make(
			EDLErrorKind::NonDeterministic,
			TEXT("ability_catalog"),
			TEXT("Ability catalog failed to load")));
	}
	Singleton = Created;
	return Created;
}

FString UDLAbilityCatalog::ClassesDir()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DestinyLike")))
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/Classes"));
	}
	return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("DestinyLike/Config/Classes"));
}

EDLClassId UDLAbilityCatalog::ClassIdFromName(const FString& Name)
{
	if (Name.Equals(TEXT("pathfinder"), ESearchCase::IgnoreCase)) return EDLClassId::Pathfinder;
	if (Name.Equals(TEXT("warden"), ESearchCase::IgnoreCase)) return EDLClassId::Warden;
	return EDLClassId::Vanguard;
}

UClass* UDLAbilityCatalog::ExpectedSlotBase(EDLAbilitySlot Slot)
{
	switch (Slot)
	{
	case EDLAbilitySlot::Grenade: return UDLGrenadeAbility::StaticClass();
	case EDLAbilitySlot::Shield: return UDLShieldAbility::StaticClass();
	case EDLAbilitySlot::Evasion: return UDLEvasionAbility::StaticClass();
	case EDLAbilitySlot::Dash: return UDLDashAbility::StaticClass();
	case EDLAbilitySlot::Melee: return UDLMeleeAbility::StaticClass();
	case EDLAbilitySlot::Jump: return UDLJumpAbility::StaticClass();
	case EDLAbilitySlot::Super: return UDLSuperAbility::StaticClass();
	default: return UDLAbility::StaticClass();
	}
}

UClass* UDLAbilityCatalog::FindType(const FString& TypeName) const
{
	if (UClass* Found = FDLAbilityTypeRegistry::Find(TypeName))
	{
		return Found;
	}
	TArray<UClass*> Derived;
	GetDerivedClasses(UDLAbility::StaticClass(), Derived, true);
	for (UClass* Class : Derived)
	{
		if (Class && !Class->HasAnyClassFlags(CLASS_Abstract) && Class->GetName() == TypeName)
		{
			FDLAbilityTypeRegistry::Register(TypeName, Class);
			return Class;
		}
	}
	return nullptr;
}

TSharedPtr<FJsonObject> UDLAbilityCatalog::MergeFields(const TSharedPtr<FJsonObject>& Base, const TSharedPtr<FJsonObject>& Overlay)
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

bool UDLAbilityCatalog::LoadFiles()
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLAbilityFeelSettings"), TEXT("AbilityCooldownScale"), CooldownScale, GGameIni);

	Abilities.Reset();
	Classes.Reset();

	FString CatalogText;
	const FString CatalogPath = FPaths::Combine(ClassesDir(), TEXT("AbilityCatalog.json"));
	if (!FFileHelper::LoadFileToString(CatalogText, *CatalogPath))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: missing ability catalog %s"), *CatalogPath);
		return false;
	}

	TSharedPtr<FJsonObject> CatalogObj;
	const TSharedRef<TJsonReader<>> CatalogReader = TJsonReaderFactory<>::Create(CatalogText);
	if (!FJsonSerializer::Deserialize(CatalogReader, CatalogObj) || !CatalogObj.IsValid())
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: failed to parse AbilityCatalog.json"));
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
			if (!DLParseAbilitySlot(Obj->GetStringField(TEXT("slot")), Def.Slot))
			{
				UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: ability %s has invalid slot"), *Def.Id.ToString());
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
			UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: missing class kit %s"), *Path);
			continue;
		}
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: failed to parse class kit %s"), *Path);
			UDLErrorBoundary::ReportStatic(this, FDLError::Make(EDLErrorKind::NonDeterministic, TEXT("class_kit_parse"), Path));
			continue;
		}
		FClassDef Def;
		Def.Id = Obj->GetStringField(TEXT("id"));
		Def.CharacterClass = Obj->GetStringField(TEXT("characterClass"));
		if (const TSharedPtr<FJsonObject> Slots = Obj->GetObjectField(TEXT("slots")))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Slots->Values)
			{
				EDLAbilitySlot Slot;
				if (!DLParseAbilitySlot(Pair.Key, Slot))
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

UDLAbility* UDLAbilityCatalog::SpawnBoundAbility(UObject* Outer, EDLClassId ClassId, EDLAbilitySlot Slot) const
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
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: class %s slot missing catalog id %s"), *ClassDef->Id, *AbilityId->ToString());
		return nullptr;
	}
	if (AbilityDef->Slot != Slot)
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: %s is a %d ability, cannot bind to slot %d"), *AbilityId->ToString(), static_cast<int32>(AbilityDef->Slot), static_cast<int32>(Slot));
		return nullptr;
	}

	UClass* Type = FindType(AbilityDef->Type);
	UClass* SlotBase = ExpectedSlotBase(Slot);
	if (!Type || !Type->IsChildOf(SlotBase))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: type %s is not valid for slot %d"), *AbilityDef->Type, static_cast<int32>(Slot));
		return nullptr;
	}

	UDLAbility* Inst = NewObject<UDLAbility>(Outer, Type);
	Inst->Id = AbilityDef->Id;
	Inst->Slot = Slot;
	const TSharedPtr<FJsonObject>* Args = ClassDef->SlotArgs.Find(Slot);
	Inst->ApplyTuning(MergeFields(AbilityDef->Fields, Args ? *Args : nullptr), CooldownScale);
	return Inst;
}

FString UDLAbilityCatalog::GetCharacterClassName(EDLClassId ClassId) const
{
	if (const FClassDef* ClassDef = Classes.Find(ClassId))
	{
		return ClassDef->CharacterClass;
	}
	return FString();
}

TSubclassOf<APawn> UDLAbilityCatalog::ResolvePawnClass(EDLClassId ClassId) const
{
	switch (ClassId)
	{
	case EDLClassId::Pathfinder: return ADLPathfinderCharacter::StaticClass();
	case EDLClassId::Warden: return ADLWardenCharacter::StaticClass();
	default: return ADLVanguardCharacter::StaticClass();
	}
}
