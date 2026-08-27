#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "CLNavArea_LongJump.generated.h"

/** Expensive hop (triple-jump over a full wall). Recast can use it; walking around is cheaper. */
UCLASS()
class CALLING_API UCLNavArea_LongJump : public UNavArea
{
	GENERATED_BODY()

public:
	UCLNavArea_LongJump();
};
