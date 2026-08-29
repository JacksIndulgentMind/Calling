#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/CLLobbyTypes.h"
#include "CLSeatRegistry.generated.h"

class UCLParticipantSeat;
class UCLRemoteAgentSeatMotor;
class ACLPlayerCharacter;
class APawn;
class AController;
class APlayerController;
class AActor;

UCLASS()
class CALLING_API UCLSeatRegistry : public UObject
{
	GENERATED_BODY()

public:
	void Reset();
	TArray<UCLParticipantSeat*> GetAll() const;
	int32 Num() const;

	UCLParticipantSeat* Find(const FGuid& SeatId) const;
	UCLParticipantSeat* FindByName(const FString& DisplayName) const;
	UCLParticipantSeat* FindHost() const;
	UCLParticipantSeat* FindLocal() const;
	UCLParticipantSeat* FindForController(const AController* Controller) const;
	APawn* GetDrivenPawn(const FGuid& SeatId) const;
	bool IsRemotelyDriven(const APawn* Pawn) const;
	ACLPlayerCharacter* FindHumanPawn() const;
	int32 ReadyCount() const;

	UCLParticipantSeat* MakeSeat(const FString& DisplayName, UClass* MotorClass, const FGuid& ExistingId, const FCLLobbyGate* Gate);
	UCLParticipantSeat* EnsureLocalHuman(const FString& ProfileName, const FCLLobbyGate* Gate);
	UCLParticipantSeat* EnsureNetHuman(APlayerController* PC, const FString& ProfileName, const FCLLobbyGate* Gate);
	void RemoveForController(AController* Controller);
	static UClass* SeatMotorClassFromKind(const FString& Kind);

	APawn* SpawnAgentPawn(ECLPvpTeam Team) const;
	AActor* FindTeamPlayerStart(ECLPvpTeam Team) const;

	TArray<TObjectPtr<UCLParticipantSeat>>& MutableSeats() { return Seats; }

protected:
	UPROPERTY()
	TArray<TObjectPtr<UCLParticipantSeat>> Seats;
};
