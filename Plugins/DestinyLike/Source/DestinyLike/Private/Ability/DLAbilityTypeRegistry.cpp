#include "Ability/DLAbilityTypeRegistry.h"

namespace
{
	TMap<FString, UClass*>& Map()
	{
		static TMap<FString, UClass*> Types;
		return Types;
	}
}

void FDLAbilityTypeRegistry::Register(const FString& TypeName, UClass* Class)
{
	if (!TypeName.IsEmpty() && Class)
	{
		Map().Add(TypeName, Class);
	}
}

UClass* FDLAbilityTypeRegistry::Find(const FString& TypeName)
{
	if (UClass* const* Found = Map().Find(TypeName))
	{
		return *Found;
	}
	return nullptr;
}
