#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Game/DLLobbyTypes.h"
#include "Game/DLAgentSequenceRunner.h"
#include "Dom/JsonObject.h"
#include "DLLobbySubsystem.generated.h"

class UDLParticipantSeat;
class UDLInvoiceBox;
class UDLGateBox;
class ADLPlayerCharacter;
class APawn;
class AController;
class AActor;

/**
 * Present-or-absent invoice / gate / seats. Destination consumes the invoice.
 * No gate means load post-lobby. No invoice means no seat list (Practice).
 */
UCLASS()
class DESTINYLIKE_API UDLLobbySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void SetPendingInvoice(const FDLLobbyInvoice& Invoice);
	void ClearPendingInvoice();
	const FDLLobbyInvoice* GetPendingInvoice() const;

	void BeginGatedScene(EDLSceneId Scene);
	void BeginOpenScene();
	void BeginComposerScene();
	void BeginPvpOrRestore();
	void ClearScene();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	bool IsGameplayUnlocked() const { return bGameplayUnlocked; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	bool HasInvoice() const { return Invoice != nullptr; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Lobby")
	bool HasGate() const { return Gate != nullptr; }

	const FDLLobbyInvoice* GetInvoice() const;
	const FDLLobbyGate* GetGate() const;
	FName GetLootRealmId() const;

	UDLParticipantSeat* FindSeat(const FGuid& SeatId) const;
	UDLParticipantSeat* FindSeatByName(const FString& DisplayName) const;
	UDLParticipantSeat* FindHostSeat() const;
	UDLParticipantSeat* FindLocalSeat() const;
	UDLParticipantSeat* FindSeatForController(const AController* Controller) const;
	APawn* GetDrivenPawn(const FGuid& SeatId) const;
	bool IsRemotelyDriven(const APawn* Pawn) const;
	ADLPlayerCharacter* FindHumanPawn() const;
	bool IsReadyLocked() const;
	bool IsMatchStartQueued() const { return bMatchStartQueued; }
	bool IsCountdownRunning() const { return bCountdownRunning; }
	float GetCountdownRemaining() const { return CountdownRemaining; }

	int32 ReadyCount() const;
	TArray<UDLParticipantSeat*> GetSeats() const;

	UDLParticipantSeat* EnsureLocalHumanSeat();
	UDLParticipantSeat* JoinRemoteAgent(const FString& DisplayName, bool bHeadless, FString& OutError, const FString& Kind = TEXT("remoteAgent"));
	bool SetReady(const FGuid& SeatId, bool bReady);
	bool ToggleLocalReady();
	bool ClaimLocalHost();
	bool ClaimLocalGuest();
	bool SetTeam(const FGuid& SeatId, EDLPvpTeam Team, FString& OutError);
	bool RequestGo();
	bool RequestLocalGo();
	bool MindControl(const FGuid& AgentSeatId, const FGuid& TargetSeatId, FString& OutError);
	bool QueuePlan(const FGuid& SeatId, const TArray<FDLAgentStep>& Steps, bool bRemainder, FString& OutError);
	bool StartGoto(const FGuid& SeatId, const FVector& Dest, FString& OutError);
	bool SetViewSeat(const FGuid& SeatId, FString& OutError);
	APawn* GetDemoViewPawn() const { return LastDemoViewPawn.Get(); }
	void StampRosterOntoInvoice();
	void RestoreBodiesAfterTravel();
	void NotifyHubSnapshots(EDLHubSnapshotReason Reason, const FGuid& OnlySeat = FGuid());

	TSharedRef<FJsonObject> HandleMessage(const TSharedPtr<FJsonObject>& Root);
	void FillStateJson(const TSharedRef<FJsonObject>& Root) const;

	FGuid GetLastJoinedSeatId() const { return LastJoinedSeatId; }

protected:
	void BindClock();
	void UnbindClock();
	void TickNet(float DeltaSeconds);
	void ConsumePendingOrDefault(EDLSceneId Scene);
	void InstallGateFromConfig();
	void LoadCountdownFromConfig();
	void StartCountdownIfReady();
	void FinishGo();
	void CheckMinPlayers();
	UDLParticipantSeat* MakeSeat(const FString& DisplayName, UClass* PlaybookClass, const FGuid& ExistingId = FGuid());
	APawn* SpawnAgentPawn(EDLPvpTeam Team) const;
	AActor* FindTeamPlayerStart(EDLPvpTeam Team) const;
	void RecreateSeatsFromRoster();
	static FString AccessName(EDLLobbyAccess Access);
	static FString TeamName(EDLPvpTeam Team);
	static EDLPvpTeam ParseTeam(const FString& Text);
	static UClass* PlaybookClassFromKind(const FString& Kind);

	UPROPERTY()
	TObjectPtr<UDLInvoiceBox> PendingInvoice;

	UPROPERTY()
	TObjectPtr<UDLInvoiceBox> Invoice;

	UPROPERTY()
	TObjectPtr<UDLGateBox> Gate;

	UPROPERTY()
	TArray<TObjectPtr<UDLParticipantSeat>> Seats;

	UPROPERTY()
	bool bGameplayUnlocked = true;

	UPROPERTY()
	float CountdownRemaining = 0.f;

	UPROPERTY()
	bool bCountdownRunning = false;

	UPROPERTY()
	bool bMatchStartQueued = false;

	UPROPERTY()
	float LaunchCountdownSeconds = 1.f;

	UPROPERTY()
	FGuid LastJoinedSeatId;

	TWeakObjectPtr<APawn> LastDemoViewPawn;

	FDelegateHandle NetTickHandle;
};
