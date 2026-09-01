#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Tagged encounter strategy. Factory is type string; tick is visitor/strategy. No mode `kind` switch. */
class ICLEncounterRules
{
public:
	virtual ~ICLEncounterRules() = default;
	virtual FName GetType() const = 0;
	virtual FName GetId() const = 0;
};

struct FCLShrineClashEncounter final : public ICLEncounterRules
{
	FName Id;
	int32 TeamFinalBlows = 10;
	FName OccupyTag;
	float RotateSeconds = 45.f;
	bool bStealIfTenWithoutShrine = true;
	bool bFailIfEitherTeamKillsZero = true;
	float FailTimeoutSeconds = 480.f;

	virtual FName GetType() const override { return FName(TEXT("shrineClash")); }
	virtual FName GetId() const override { return Id; }
};

struct FCLOffVolumeDot
{
	bool bEnabled = false;
	FName Type = FName(TEXT("damageOverTime"));
	float GraceSeconds = 8.f;
	float DamagePerSecond = 12.f;
};

struct FCLSpawnerPoolEntry
{
	FName Bot;
	float Weight = 1.f;
};

struct FCLSpawnerDef
{
	FName OriginMarker;
	/** Horizontal disk around the origin marker (cm). `jitterCm` in JSON is an alias. */
	float RadiusCm = 0.f;
	/** Capsule radius that must be empty of pawns / walls (cm). 0 = combat pawn capsule. A miss is `raid_spawn_unclear`, not a skip. */
	float ClearRadiusCm = 0.f;
	/** Candidate samples in the disk per bot. 0 = 12. Exhausting them fails the match. */
	int32 ClearTries = 12;
	TArray<FCLSpawnerPoolEntry> Pool;
	int32 CountMin = 1;
	int32 CountMax = 1;
	float IntervalSeconds = 8.f;
	int32 Waves = 1;
};

struct FCLWaveHoldPhase
{
	FName Id;
	FName OccupyMarker;
	FCLOffVolumeDot OffVolume;
	FCLSpawnerDef Spawner;
};

struct FCLWaveHoldEncounter final : public ICLEncounterRules
{
	FName Id;
	TArray<FCLWaveHoldPhase> Phases;
	FName OpensMarker;

	virtual FName GetType() const override { return FName(TEXT("waveHold")); }
	virtual FName GetId() const override { return Id; }
};

TSharedPtr<ICLEncounterRules> CLParseEncounter(const TSharedPtr<FJsonObject>& Obj, FString& OutError);
