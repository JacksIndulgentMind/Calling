#pragma once

#include "CoreMinimal.h"
#include "Player/DLPlayerCharacter.h"
#include "DLPathfinderCharacter.generated.h"

UCLASS()
class DESTINYLIKE_API ADLPathfinderCharacter : public ADLPlayerCharacter
{
	GENERATED_BODY()
public:
	ADLPathfinderCharacter(const FObjectInitializer& ObjectInitializer);
};
