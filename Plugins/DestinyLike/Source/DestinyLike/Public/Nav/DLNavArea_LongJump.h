#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "DLNavArea_LongJump.generated.h"

/** Expensive hop (triple-jump over a full wall). Recast can use it; walking around is cheaper. */
UCLASS()
class DESTINYLIKE_API UDLNavArea_LongJump : public UNavArea
{
	GENERATED_BODY()

public:
	UDLNavArea_LongJump();
};
