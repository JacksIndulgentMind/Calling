#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/DLTypes.h"
#include "DLGameInstance.generated.h"

class UDLProfileSubsystem;
class UDLVaultSubsystem;
class UDLSessionSubsystem;
class UDLSceneRouter;
class UDLTickSubsystem;
class UDLLootRulesService;
class UDLLobbySubsystem;

/**
 * Project GameInstance. Wires profile, vault, session, scene router, and tick clock.
 */
UCLASS(Blueprintable)
class DESTINYLIKE_API UDLGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLProfileSubsystem* GetProfileSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLVaultSubsystem* GetVaultSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLSessionSubsystem* GetSessionSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLSceneRouter* GetSceneRouter() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLTickSubsystem* GetTickSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLLootRulesService* GetLootRulesService() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLLobbySubsystem* GetLobbySubsystem() const;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void SetLastSocialMap(const FString& MapName);

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	FString GetLastSocialMap() const { return LastSocialMap; }

protected:
	UPROPERTY()
	FString LastSocialMap;

	UPROPERTY()
	TObjectPtr<UDLLootRulesService> LootRulesService = nullptr;
};
