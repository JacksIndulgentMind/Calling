#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DLTypes.h"
#include "TimerManager.h"
#include "DLSceneRouter.generated.h"

/**
 * Travels between Boot / Social / PvP / Raid / Practice.
 * Exit-from-activity always returns to last Social (or default Social map).
 */
UCLASS()
class DESTINYLIKE_API UDLSceneRouter : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Scene")
	void TravelToScene(EDLSceneId Scene, int32 RaidChamberIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Scene")
	void ExitActivityToSocial();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Scene")
	void RememberSocialMap(const FString& MapName);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Scene")
	FString GetMapNameForScene(EDLSceneId Scene, int32 RaidChamberIndex = 0) const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Scene")
	EDLSceneId GetCurrentScene() const { return CurrentScene; }

	void SetCurrentScene(EDLSceneId Scene) { CurrentScene = Scene; }

private:
	void ReloadMapNamesFromConfig();
	void SoftTravel(const FString& MapName, EDLSceneId Scene);
	void TravelDeferred(const FString& MapName, EDLSceneId Scene);
	static FString GameModePathForScene(EDLSceneId Scene);

	FTimerHandle DeferredTravelTimer;

	UPROPERTY()
	EDLSceneId CurrentScene = EDLSceneId::Boot;

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
