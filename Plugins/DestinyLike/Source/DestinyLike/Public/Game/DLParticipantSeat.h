#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/DLLobbyTypes.h"
#include "DLParticipantSeat.generated.h"

class UDLControllerPlaybook;
class UDLPossessionComponent;
class APawn;
class AActor;

UCLASS()
class DESTINYLIKE_API UDLParticipantSeat : public UObject
{
	GENERATED_BODY()

public:
	void Configure(const FGuid& InSeatId, const FString& InDisplayName, UDLControllerPlaybook* InPlaybook);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	FGuid GetSeatId() const { return SeatId; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	FString GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	bool IsHost() const { return bHost; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	EDLPvpTeam GetTeam() const { return Team; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	bool IsHeadlessJoin() const { return bHeadlessJoin; }

	FGuid GetDriveSeatId() const { return DriveSeatId; }

	UDLControllerPlaybook* GetPlaybook() const { return Playbook; }
	UDLPossessionComponent* GetPossession() const { return Possession; }
	AActor* GetAnchor() const { return Anchor.Get(); }
	APawn* GetDrivenPawn() const;

	void SetReady(bool bInReady) { bReady = bInReady; }
	void SetHost(bool bInHost) { bHost = bInHost; }
	void SetTeam(EDLPvpTeam InTeam) { Team = InTeam; }
	void SetHeadlessJoin(bool bInHeadless) { bHeadlessJoin = bInHeadless; }
	void SetDriveSeatId(const FGuid& InDriveSeatId) { DriveSeatId = InDriveSeatId; }
	void SetPossession(UDLPossessionComponent* InPossession) { Possession = InPossession; }
	void SetAnchor(AActor* InAnchor) { Anchor = InAnchor; }

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
	EDLPvpTeam Team = EDLPvpTeam::Unassigned;

	UPROPERTY()
	bool bHeadlessJoin = false;

	UPROPERTY()
	FGuid DriveSeatId;

	UPROPERTY()
	TObjectPtr<UDLControllerPlaybook> Playbook;

	UPROPERTY()
	TObjectPtr<UDLPossessionComponent> Possession;

	UPROPERTY()
	TWeakObjectPtr<AActor> Anchor;
};
