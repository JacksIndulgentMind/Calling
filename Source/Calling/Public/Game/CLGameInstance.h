#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/CLTypes.h"
#include "CLGameInstance.generated.h"

class UCLProfileSubsystem;
class UCLVaultSubsystem;
class UCLSessionSubsystem;
class UCLSceneRouter;
class UCLTickSubsystem;
class UCLLootRulesService;
class UCLLobbySubsystem;

/**
 * Project GameInstance. Wires profile, vault, session, scene router, and tick clock.
 */
UCLASS(Blueprintable)
class CALLING_API UCLGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLProfileSubsystem* GetProfileSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLVaultSubsystem* GetVaultSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLSessionSubsystem* GetSessionSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLSceneRouter* GetSceneRouter() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLTickSubsystem* GetTickSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLLootRulesService* GetLootRulesService() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLLobbySubsystem* GetLobbySubsystem() const;

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void SetLastSocialMap(const FString& MapName);

	UFUNCTION(BlueprintPure, Category = "Calling")
	FString GetLastSocialMap() const { return LastSocialMap; }

protected:
	UPROPERTY()
	FString LastSocialMap;

	UPROPERTY()
	TObjectPtr<UCLLootRulesService> LootRulesService = nullptr;
};
