#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "CLNavArea_AirDive.generated.h"

/** Generated off-mesh air-dive. Prefer walk. Search radius is the ability envelope. */
UCLASS()
class CALLING_API UCLNavArea_AirDive : public UNavArea
{
	GENERATED_BODY()

public:
	UCLNavArea_AirDive();
};
