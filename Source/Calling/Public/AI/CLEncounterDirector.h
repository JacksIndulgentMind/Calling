#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/CLEncounterRules.h"
#include "CLEncounterDirector.generated.h"

class UCLParticipantSeat;
class UCLStatusEffectComponent;
class ACLCombatPawn;
class ACLGreyboxFloors;

UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLEncounterDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLEncounterDirector();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BindWaveHold(const TArray<const FCLWaveHoldEncounter*>& Encounters);
	void BeginFirstEncounter();
	void ClearSpawned();
	bool IsFinished() const { return bFinished; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Raid")
	float ArenaHalfExtent = 5000.f;

protected:
	struct FSpawnedNpc
	{
		TWeakObjectPtr<APawn> Pawn;
		TObjectPtr<UCLParticipantSeat> Seat;
	};

	void StartEncounter(int32 Index);
	void StartPhase(int32 PhaseIndex);
	void SpawnWave();
	bool HasNavTiles() const;
	FName RollBot(const FCLSpawnerDef& Spawner) const;
	bool FindClearSpawnLocation(const FVector& Origin, const FCLSpawnerDef& Spawner, FVector& OutStand, FString& OutReason) const;
	APawn* SpawnBot(FName BotId, const FVector& Location);
	void FailSpawn(const TCHAR* Code, const FString& Detail);
	void TickNpcs(float DeltaSeconds);
	void SweepDead();
	void CompletePhase();
	void CompleteEncounter();
	void ApplyOffVolume(const FCLWaveHoldPhase& Phase);
	UCLStatusEffectComponent* EnsureStatus(APawn* Pawn) const;

	TArray<const FCLWaveHoldEncounter*> WaveHolds;
	int32 EncounterIndex = 0;
	int32 PhaseIndex = 0;
	int32 WavesDone = 0;
	int32 WavesSpawned = 0;
	float WaveTimer = 0.f;
	bool bRunning = false;
	bool bFinished = false;
	bool bWaveInFlight = false;
	bool bAwaitingNav = false;

	UPROPERTY()
	TArray<TObjectPtr<UCLParticipantSeat>> NpcSeats;

	TArray<FSpawnedNpc> Spawned;
};
