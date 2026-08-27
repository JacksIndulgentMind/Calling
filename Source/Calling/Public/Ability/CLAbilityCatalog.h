#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/CLAbilityTypes.h"
#include "Core/CLTypes.h"
#include "CLAbilityCatalog.generated.h"

class UCLAbility;
class FJsonObject;

UCLASS()
class CALLING_API UCLAbilityCatalog : public UObject
{
	GENERATED_BODY()

public:
	static UCLAbilityCatalog* Get(UObject* WorldContext);

	bool LoadFiles();
	UCLAbility* SpawnBoundAbility(UObject* Outer, ECLClassId ClassId, ECLAbilitySlot Slot) const;
	FString GetCharacterClassName(ECLClassId ClassId) const;
	TSubclassOf<APawn> ResolvePawnClass(ECLClassId ClassId) const;
	static FString ClassesDir();

protected:
	struct FAbilityDef
	{
		FName Id;
		FString Type;
		ECLAbilitySlot Slot = ECLAbilitySlot::Grenade;
		TSharedPtr<FJsonObject> Fields;
	};

	struct FClassDef
	{
		FString Id;
		FString CharacterClass;
		TMap<ECLAbilitySlot, FName> SlotAbility;
		TMap<ECLAbilitySlot, TSharedPtr<FJsonObject>> SlotArgs;
	};

	TMap<FName, FAbilityDef> Abilities;
	TMap<ECLClassId, FClassDef> Classes;
	float CooldownScale = 0.15f;
	bool bLoaded = false;

	UClass* FindType(const FString& TypeName) const;
	static UClass* ExpectedSlotBase(ECLAbilitySlot Slot);
	static ECLClassId ClassIdFromName(const FString& Name);
	static TSharedPtr<FJsonObject> MergeFields(const TSharedPtr<FJsonObject>& Base, const TSharedPtr<FJsonObject>& Overlay);
};
