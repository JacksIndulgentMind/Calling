#pragma once

#include "CoreMinimal.h"
#include "Player/CLVanguardCharacter.h"
#include "CLCombatPawn.generated.h"

/** NPC combat body: same guns and intent seam as players, wider avatar, algorithmic AI by default. */
UCLASS()
class CALLING_API ACLCombatPawn : public ACLVanguardCharacter
{
	GENERATED_BODY()

public:
	ACLCombatPawn(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void SetDemoViewActive(bool bActive) override;
};
