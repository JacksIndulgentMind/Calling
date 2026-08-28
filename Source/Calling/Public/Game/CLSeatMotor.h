#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Input/CLAgentIntent.h"
#include "Game/CLAgentSequenceRunner.h"
#include "Game/CLAgentGotoDriver.h"
#include "Game/CLLobbyTypes.h"
#include "CLSeatMotor.generated.h"

class UCLParticipantSeat;
class ACLPlayerCharacter;
class UWorld;

/** Who drives a seat. Not a BotBook — humans are not bots. */
UCLASS(Abstract)
class CALLING_API UCLSeatMotor : public UObject
{
	GENERATED_BODY()

public:
	virtual void TickNet(float DeltaSeconds, UCLParticipantSeat* Seat);
	virtual FString GetKindId() const { return TEXT("none"); }
	virtual bool WantsHubSnapshot(ECLHubSnapshotReason Reason) const;

	bool StartGoto(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError);
	void CancelGoto();
	bool IsGotoActive() const { return Goto.bActive; }
	bool IsGotoPartial() const { return Goto.bPartial; }
	FVector GetGotoGoal() const { return Goto.Goal; }
	int32 GetGotoWaypointCount() const { return Goto.Path.Num(); }
	FCLAgentGotoDriver& GetGotoDriver() { return Goto; }
	const FCLAgentGotoDriver& GetGotoDriver() const { return Goto; }

protected:
	FCLAgentGotoDriver Goto;
};

UCLASS()
class CALLING_API UCLHumanSeatMotor : public UCLSeatMotor
{
	GENERATED_BODY()

public:
	virtual FString GetKindId() const override { return TEXT("human"); }
};

UCLASS()
class CALLING_API UCLAlgorithmicSeatMotor : public UCLSeatMotor
{
	GENERATED_BODY()

public:
	virtual void TickNet(float DeltaSeconds, UCLParticipantSeat* Seat) override;
	virtual FString GetKindId() const override { return TEXT("algorithmic"); }
};

UCLASS()
class CALLING_API UCLRemoteAgentSeatMotor : public UCLSeatMotor
{
	GENERATED_BODY()

public:
	virtual void TickNet(float DeltaSeconds, UCLParticipantSeat* Seat) override;
	virtual FString GetKindId() const override { return TEXT("remoteAgent"); }
	virtual bool WantsHubSnapshot(ECLHubSnapshotReason Reason) const override;

	bool QueuePlan(const TArray<FCLAgentStep>& Steps, bool bRemainder, FString& OutError);
	void MarkReply();
	void CancelPlan();
	void CancelMotor();

	float RemainingSeconds() const { return Plan.RemainingSeconds(); }
	int32 GetQueuedStepCount() const { return Plan.Steps.Num(); }
	int32 GetSequenceIndex() const { return Plan.IsActive() ? Plan.Index : -1; }

	bool NeedsReplan() const { return bNeedsReplan; }
	void SetStaleSeconds(float Seconds) { StaleSeconds = FMath::Max(0.25f, Seconds); }
	void SetLookaheadSeconds(float Seconds) { LookaheadSeconds = FMath::Max(0.1f, Seconds); }
	bool ConsumePendingSnapshot(ECLHubSnapshotReason& OutReason);

protected:
	FCLAgentSequenceRunner Plan;
	double LastReplySeconds = 0.0;
	float StaleSeconds = 3.f;
	float LookaheadSeconds = 0.75f;
	bool bNeedsReplan = false;
	bool bLookaheadPushed = false;
	bool bPendingStale = false;
	bool bPendingLookahead = false;

	void ApplyLookStep(ACLPlayerCharacter* Char, const FCLAgentStep& Step) const;
	bool TickBotBook(float DeltaSeconds, UCLParticipantSeat* Seat);
};

/** Cursor MCP connector: same motor as remoteAgent, hub snapshots only when stale. */
UCLASS()
class CALLING_API UCLCursorSeatMotor : public UCLRemoteAgentSeatMotor
{
	GENERATED_BODY()

public:
	virtual FString GetKindId() const override { return TEXT("cursor"); }
	virtual bool WantsHubSnapshot(ECLHubSnapshotReason Reason) const override;
};
