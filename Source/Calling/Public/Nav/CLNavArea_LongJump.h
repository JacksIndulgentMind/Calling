#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "CLNavArea_LongJump.generated.h"

/** Expensive hop. DefaultCost from NavTune areaCost.longJump (fallback 25). */
UCLASS()
class CALLING_API UCLNavArea_LongJump : public UNavArea
{
	GENERATED_BODY()

public:
	UCLNavArea_LongJump();
};
