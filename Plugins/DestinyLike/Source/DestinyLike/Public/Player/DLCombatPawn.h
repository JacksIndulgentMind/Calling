#pragma once

#include "CoreMinimal.h"
#include "Player/DLVanguardCharacter.h"
#include "DLCombatPawn.generated.h"

/** NPC combat body: same guns and intent seam as players, wider avatar, algorithmic AI by default. */
UCLASS()
class DESTINYLIKE_API ADLCombatPawn : public ADLVanguardCharacter
{
	GENERATED_BODY()

public:
	ADLCombatPawn(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void SetDemoViewActive(bool bActive) override;
};
