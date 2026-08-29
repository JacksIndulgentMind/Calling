#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "CLNavArea_AirDive.generated.h"

/** Generated off-mesh air-dive. DefaultCost 50 — prefer walk/DropDown when both exist (LongJump is 25). */
UCLASS()
class CALLING_API UCLNavArea_AirDive : public UNavArea
{
	GENERATED_BODY()

public:
	UCLNavArea_AirDive();
};
