#pragma once

#include "CoreMinimal.h"

class UClass;

struct DESTINYLIKE_API FDLAbilityTypeRegistry
{
	static void Register(const FString& TypeName, UClass* Class);
	static UClass* Find(const FString& TypeName);
};
