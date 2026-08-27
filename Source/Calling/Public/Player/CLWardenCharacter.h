#pragma once

#include "CoreMinimal.h"
#include "Player/CLPlayerCharacter.h"
#include "CLWardenCharacter.generated.h"

UCLASS()
class CALLING_API ACLWardenCharacter : public ACLPlayerCharacter
{
	GENERATED_BODY()
public:
	ACLWardenCharacter(const FObjectInitializer& ObjectInitializer);
};
