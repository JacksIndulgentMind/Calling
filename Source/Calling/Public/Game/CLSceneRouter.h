#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CLTypes.h"
#include "TimerManager.h"
#include "CLSceneRouter.generated.h"

/**
 * Travels between Boot / Social / PvP / Raid / Practice.
 * Exit-from-activity always returns to last Social (or default Social map).
 */
UCLASS()
class CALLING_API UCLSceneRouter : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Scene")
	void TravelToScene(ECLSceneId Scene, int32 RaidChamberIndex = 0, bool bListen = false);

	UFUNCTION(BlueprintCallable, Category = "Calling|Scene")
	void ExitActivityToSocial();

	UFUNCTION(BlueprintCallable, Category = "Calling|Scene")
	void RememberSocialMap(const FString& MapName);

	UFUNCTION(BlueprintPure, Category = "Calling|Scene")
	FString GetMapNameForScene(ECLSceneId Scene, int32 RaidChamberIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "Calling|Scene")
	ECLSceneId GetCurrentScene() const { return CurrentScene; }

	void SetCurrentScene(ECLSceneId Scene) { CurrentScene = Scene; }

private:
	void ReloadMapNamesFromConfig();
	void SoftTravel(const FString& MapName, ECLSceneId Scene, bool bListen);
	void TravelDeferred(const FString& MapName, ECLSceneId Scene, bool bListen);
	static FString GameModePathForScene(ECLSceneId Scene);

	FTimerHandle DeferredTravelTimer;

	UPROPERTY()
	ECLSceneId CurrentScene = ECLSceneId::Boot;

	UPROPERTY()
	FString BootMapName;

	UPROPERTY()
	FString SocialMapName;

	UPROPERTY()
	FString PvpMapName;

	UPROPERTY()
	FString PracticeMapName;

	UPROPERTY()
	TArray<FString> RaidChamberMapNames;

	UPROPERTY()
	FString LastSocialMapName;
};
