#pragma once

#include "CoreMinimal.h"
#include "Player/DLPlayerCharacter.h"
#include "DLVanguardCharacter.generated.h"

UCLASS()
class DESTINYLIKE_API ADLVanguardCharacter : public ADLPlayerCharacter
{
	GENERATED_BODY()
public:
	ADLVanguardCharacter(const FObjectInitializer& ObjectInitializer);
};
