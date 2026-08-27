#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Game/CLLobbyTypes.h"
#include "Game/CLAgentSequenceRunner.h"
#include "Dom/JsonObject.h"
#include "CLLobbySubsystem.generated.h"

class UCLParticipantSeat;
class UCLInvoiceService;
class UCLSeatRegistry;
class UCLGateCountdown;
class UCLTravelCoordinator;
class ACLPlayerCharacter;
class APawn;
class AController;
class AActor;

/**
 * Thin GameInstance façade over invoice, seats, gate, and travel collaborators.
 */
UCLASS()
class CALLING_API UCLLobbySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SetPendingInvoice(const FCLLobbyInvoice& Invoice);
	void ClearPendingInvoice();
	const FCLLobbyInvoice* GetPendingInvoice() const;

	void BeginGatedScene(ECLSceneId Scene);
	void BeginOpenScene();
	void BeginComposerScene();
	void BeginPvpOrRestore();
	void ClearScene();

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	bool IsGameplayUnlocked() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	bool HasInvoice() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Lobby")
	bool HasGate() const;

	const FCLLobbyInvoice* GetInvoice() const;
	const FCLLobbyGate* GetGate() const;
	FName GetLootRealmId() const;

	UCLParticipantSeat* FindSeat(const FGuid& SeatId) const;
	UCLParticipantSeat* FindSeatByName(const FString& DisplayName) const;
	UCLParticipantSeat* FindHostSeat() const;
	UCLParticipantSeat* FindLocalSeat() const;
	UCLParticipantSeat* FindSeatForController(const AController* Controller) const;
	APawn* GetDrivenPawn(const FGuid& SeatId) const;
	bool IsRemotelyDriven(const APawn* Pawn) const;
	ACLPlayerCharacter* FindHumanPawn() const;
	bool IsReadyLocked() const;
	bool IsMatchStartQueued() const;
	bool IsCountdownRunning() const;
	float GetCountdownRemaining() const;

	int32 ReadyCount() const;
	TArray<UCLParticipantSeat*> GetSeats() const;

	UCLParticipantSeat* EnsureLocalHumanSeat();
	UCLParticipantSeat* JoinRemoteAgent(const FString& DisplayName, bool bHeadless, FString& OutError, const FString& Kind = TEXT("remoteAgent"));
	bool SetReady(const FGuid& SeatId, bool bReady);
	bool ToggleLocalReady();
	bool ClaimLocalHost();
	bool ClaimLocalGuest();
	bool SetTeam(const FGuid& SeatId, ECLPvpTeam Team, FString& OutError);
	bool RequestGo();
	bool RequestLocalGo();
	bool MindControl(const FGuid& AgentSeatId, const FGuid& TargetSeatId, FString& OutError);
	bool QueuePlan(const FGuid& SeatId, const TArray<FCLAgentStep>& Steps, bool bRemainder, FString& OutError);
	bool StartGoto(const FGuid& SeatId, const FVector& Dest, FString& OutError);
	bool SetViewSeat(const FGuid& SeatId, FString& OutError);
	APawn* GetDemoViewPawn() const { return LastDemoViewPawn.Get(); }
	void StampRosterOntoInvoice();
	void RestoreBodiesAfterTravel();
	void NotifyHubSnapshots(ECLHubSnapshotReason Reason, const FGuid& OnlySeat = FGuid());

	TSharedRef<FJsonObject> HandleMessage(const TSharedPtr<FJsonObject>& Root);
	void FillStateJson(const TSharedRef<FJsonObject>& Root) const;

	FGuid GetLastJoinedSeatId() const { return LastJoinedSeatId; }
	UCLParticipantSeat* FindOrCreateLoopbackSeat();

	UCLInvoiceService* GetInvoices() const { return Invoices; }
	UCLSeatRegistry* GetSeatRegistry() const { return SeatReg; }
	UCLGateCountdown* GetGateClock() const { return GateClock; }

protected:
	void BindClock();
	void UnbindClock();
	void TickNet(float DeltaSeconds);
	void ConsumePendingOrDefault(ECLSceneId Scene);
	void StartCountdownIfReady();
	void FinishGo();
	void CheckMinPlayers();
	static FString AccessName(ECLLobbyAccess Access);
	static FString TeamName(ECLPvpTeam Team);

	UPROPERTY()
	TObjectPtr<UCLInvoiceService> Invoices;

	UPROPERTY()
	TObjectPtr<UCLSeatRegistry> SeatReg;

	UPROPERTY()
	TObjectPtr<UCLGateCountdown> GateClock;

	UPROPERTY()
	TObjectPtr<UCLTravelCoordinator> Travel;

	UPROPERTY()
	FGuid LastJoinedSeatId;

	TWeakObjectPtr<APawn> LastDemoViewPawn;

	FDelegateHandle NetTickHandle;
};
