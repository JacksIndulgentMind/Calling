#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Ability/DLAbilityTypes.h"
#include "Core/DLTypes.h"
#include "DLAbilityCatalog.generated.h"

class UDLAbility;
class FJsonObject;

UCLASS()
class DESTINYLIKE_API UDLAbilityCatalog : public UObject
{
	GENERATED_BODY()

public:
	static UDLAbilityCatalog* Get(UObject* WorldContext);

	bool LoadFiles();
	UDLAbility* SpawnBoundAbility(UObject* Outer, EDLClassId ClassId, EDLAbilitySlot Slot) const;
	FString GetCharacterClassName(EDLClassId ClassId) const;
	TSubclassOf<APawn> ResolvePawnClass(EDLClassId ClassId) const;
	static FString ClassesDir();

protected:
	struct FAbilityDef
	{
		FName Id;
		FString Type;
		EDLAbilitySlot Slot = EDLAbilitySlot::Grenade;
		TSharedPtr<FJsonObject> Fields;
	};

	struct FClassDef
	{
		FString Id;
		FString CharacterClass;
		TMap<EDLAbilitySlot, FName> SlotAbility;
		TMap<EDLAbilitySlot, TSharedPtr<FJsonObject>> SlotArgs;
	};

	TMap<FName, FAbilityDef> Abilities;
	TMap<EDLClassId, FClassDef> Classes;
	float CooldownScale = 0.15f;
	bool bLoaded = false;

	UClass* FindType(const FString& TypeName) const;
	static UClass* ExpectedSlotBase(EDLAbilitySlot Slot);
	static EDLClassId ClassIdFromName(const FString& Name);
	static TSharedPtr<FJsonObject> MergeFields(const TSharedPtr<FJsonObject>& Base, const TSharedPtr<FJsonObject>& Overlay);
};
