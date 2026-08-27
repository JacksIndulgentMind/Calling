#pragma once

#include "CoreMinimal.h"
#include "Player/CLPlayerCharacter.h"
#include "CLVanguardCharacter.generated.h"

UCLASS()
class CALLING_API ACLVanguardCharacter : public ACLPlayerCharacter
{
	GENERATED_BODY()
public:
	ACLVanguardCharacter(const FObjectInitializer& ObjectInitializer);
};
