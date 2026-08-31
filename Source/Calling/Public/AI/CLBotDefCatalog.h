#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CLError.h"
#include "CLBotDefCatalog.generated.h"

class UCLBotBookManager;

struct FCLBotIntellectDef
{
	float ChangeResponseSeconds = 0.10f;
	FName BotBook;
	int32 BotBookSkill = 1;
	bool bBulkApperception = false;
};

struct FCLBotAbilityStackDef
{
	FName Kit = FName(TEXT("vanguard"));
	float CooldownScale = 1.f;
};

struct FCLBotDef
{
	FName Id;
	FName Role;
	FCLBotIntellectDef Intellect;
	FCLBotAbilityStackDef AbilityStack;
};

UCLASS()
class CALLING_API UCLBotDefCatalog : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool LoadFiles();
	const FCLBotDef* Find(FName Id) const;

protected:
	TMap<FName, FCLBotDef> Defs;
	bool bLoaded = false;

	FCLStatus ValidateDef(const FCLBotDef& Def, UCLBotBookManager* Books) const;
};
