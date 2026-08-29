#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/CLLobbyTypes.h"
#include "Game/CLSeatMotor.h"
#include "CLParticipantSeat.generated.h"
class UCLPossessionComponent;
class APawn;
class AActor;
class APlayerController;

UCLASS()
class CALLING_API UCLParticipantSeat : public UObject
{
	GENERATED_BODY()

public:
	void Configure(const FGuid& InSeatId, const FString& InDisplayName, UCLSeatMotor* InMotor);

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	FGuid GetSeatId() const { return SeatId; }

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	FString GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	bool IsHost() const { return bHost; }

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	ECLPvpTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	bool IsHeadlessJoin() const { return bHeadlessJoin; }

	FGuid GetDriveSeatId() const { return DriveSeatId; }

	UCLSeatMotor* GetSeatMotor() const { return Motor; }
	UCLPossessionComponent* GetPossession() const { return Possession; }
	AActor* GetAnchor() const { return Anchor.Get(); }
	APlayerController* GetBoundController() const { return BoundController.Get(); }
	APawn* GetDrivenPawn() const;

	void SetReady(bool bInReady) { bReady = bInReady; }
	void SetHost(bool bInHost) { bHost = bInHost; }
	void SetTeam(ECLPvpTeam InTeam) { Team = InTeam; }
	void SetHeadlessJoin(bool bInHeadless) { bHeadlessJoin = bInHeadless; }
	void SetDriveSeatId(const FGuid& InDriveSeatId) { DriveSeatId = InDriveSeatId; }
	void SetPossession(UCLPossessionComponent* InPossession) { Possession = InPossession; }
	void SetAnchor(AActor* InAnchor) { Anchor = InAnchor; }
	void BindController(APlayerController* PC) { BoundController = PC; }

protected:
	UPROPERTY()
	FGuid SeatId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	bool bReady = false;

	UPROPERTY()
	bool bHost = false;

	UPROPERTY()
	ECLPvpTeam Team = ECLPvpTeam::Unassigned;

	UPROPERTY()
	bool bHeadlessJoin = false;

	UPROPERTY()
	FGuid DriveSeatId;

	UPROPERTY()
	TObjectPtr<UCLSeatMotor> Motor;

	UPROPERTY()
	TObjectPtr<UCLPossessionComponent> Possession;

	UPROPERTY()
	TWeakObjectPtr<AActor> Anchor;

	UPROPERTY()
	TWeakObjectPtr<APlayerController> BoundController;
};
