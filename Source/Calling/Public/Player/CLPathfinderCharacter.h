#pragma once

#include "CoreMinimal.h"
#include "Player/CLPlayerCharacter.h"
#include "CLPathfinderCharacter.generated.h"

UCLASS()
class CALLING_API ACLPathfinderCharacter : public ACLPlayerCharacter
{
	GENERATED_BODY()
public:
	ACLPathfinderCharacter(const FObjectInitializer& ObjectInitializer);
};
