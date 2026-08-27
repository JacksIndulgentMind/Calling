#pragma once

#include "CoreMinimal.h"

class UClass;

struct CALLING_API FCLAbilityTypeRegistry
{
	static void Register(const FString& TypeName, UClass* Class);
	static UClass* Find(const FString& TypeName);
};
