#pragma once

#include "CoreMinimal.h"
#include "Player/DLPlayerCharacter.h"
#include "DLWardenCharacter.generated.h"

UCLASS()
class DESTINYLIKE_API ADLWardenCharacter : public ADLPlayerCharacter
{
	GENERATED_BODY()
public:
	ADLWardenCharacter(const FObjectInitializer& ObjectInitializer);
};
