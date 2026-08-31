#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CLError.h"
#include "Game/CLEncounterRules.h"
#include "CLGameModeCatalog.generated.h"

class ACLTaskMarker;

struct FCLMapCatalogEntry
{
	FName Id;
	FString Umap;
	int32 MinPlayers = 2;
	int32 MaxPlayers = 8;
	TArray<FName> SupportedGameModes;
	TMap<FName, TArray<FName>> MarkerTags;
};

struct FCLGameModeDef
{
	FName Id;
	TArray<FName> RequireTags;
	TArray<TSharedPtr<ICLEncounterRules>> Encounters;

	const FCLShrineClashEncounter* FindShrineClash() const;
	void CollectWaveHold(TArray<const FCLWaveHoldEncounter*>& Out) const;
};

/** JSON maps + game modes. Unreal AGameMode stays the scene host. */
UCLASS()
class CALLING_API UCLGameModeCatalog : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool LoadFiles();
	const FCLMapCatalogEntry* FindMap(FName Id) const;
	const FCLMapCatalogEntry* FindMapByUmap(const FString& Umap) const;
	const FCLGameModeDef* FindMode(FName Id) const;
	const FCLMapCatalogEntry* DefaultPvpMap() const { return FindMap(FName(TEXT("pvp_three_lane"))); }

	void ApplyMarkerTags(UWorld* World, FName MapId) const;
	FCLStatus Validate(UWorld* World, FName MapId, FName ModeId) const;

protected:
	TMap<FName, FCLMapCatalogEntry> Maps;
	TMap<FName, FCLGameModeDef> Modes;
	bool bLoaded = false;

	bool LoadMapsDir();
	bool LoadModesDir();
};
