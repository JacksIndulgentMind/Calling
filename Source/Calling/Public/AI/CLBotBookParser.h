#pragma once

#include "CoreMinimal.h"
#include "AI/CLBotBookTypes.h"
#include "Core/CLError.h"

struct CALLING_API FCLBotBookParser
{
	/** Restricted PlantUML activity subset. Catalog books reject xyz goto. */
	static FCLStatus Parse(const FString& Text, bool bAllowXyzGoto, FCLBotBook& OutBook, FString& OutError);
};
