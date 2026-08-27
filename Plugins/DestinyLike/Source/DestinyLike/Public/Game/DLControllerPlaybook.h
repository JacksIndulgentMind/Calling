#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Input/DLAgentIntent.h"
#include "Game/DLAgentSequenceRunner.h"
#include "Game/DLAgentGotoDriver.h"
#include "Game/DLLobbyTypes.h"
#include "DLControllerPlaybook.generated.h"

class UDLParticipantSeat;
class ADLPlayerCharacter;
class UWorld;

UCLASS(Abstract)
class DESTINYLIKE_API UDLControllerPlaybook : public UObject
{
	GENERATED_BODY()

public:
	virtual void TickNet(float DeltaSeconds, UDLParticipantSeat* Seat);
	virtual FString GetKindId() const { return TEXT("none"); }
	virtual bool WantsHubSnapshot(EDLHubSnapshotReason Reason) const;
};

UCLASS()
class DESTINYLIKE_API UDLHumanPlaybook : public UDLControllerPlaybook
{
	GENERATED_BODY()

public:
	virtual FString GetKindId() const override { return TEXT("human"); }
};

UCLASS()
class DESTINYLIKE_API UDLAlgorithmicPlaybook : public UDLControllerPlaybook
{
	GENERATED_BODY()

public:
	virtual FString GetKindId() const override { return TEXT("algorithmic"); }
};

UCLASS()
class DESTINYLIKE_API UDLRemoteAgentPlaybook : public UDLControllerPlaybook
{
	GENERATED_BODY()

public:
	virtual void TickNet(float DeltaSeconds, UDLParticipantSeat* Seat) override;
	virtual FString GetKindId() const override { return TEXT("remoteAgent"); }
	virtual bool WantsHubSnapshot(EDLHubSnapshotReason Reason) const override;

	bool QueuePlan(const TArray<FDLAgentStep>& Steps, bool bRemainder, FString& OutError);
	bool StartGoto(UWorld* World, ADLPlayerCharacter* Char, const FVector& Dest, FString& OutError);
	void MarkReply();
	void CancelPlan();
	void CancelGoto();

	float RemainingSeconds() const { return Plan.RemainingSeconds(); }
	bool NeedsReplan() const { return bNeedsReplan; }
	void SetStaleSeconds(float Seconds) { StaleSeconds = FMath::Max(0.25f, Seconds); }
	void SetLookaheadSeconds(float Seconds) { LookaheadSeconds = FMath::Max(0.1f, Seconds); }
	bool IsGotoActive() const { return Goto.bActive; }
	bool IsGotoPartial() const { return Goto.bPartial; }
	FVector GetGotoGoal() const { return Goto.Goal; }
	int32 GetGotoWaypointCount() const { return Goto.Path.Num(); }
	bool ConsumePendingSnapshot(EDLHubSnapshotReason& OutReason);

protected:
	FDLAgentSequenceRunner Plan;
	FDLAgentGotoDriver Goto;
	double LastReplySeconds = 0.0;
	float StaleSeconds = 3.f;
	float LookaheadSeconds = 0.75f;
	bool bNeedsReplan = false;
	bool bLookaheadPushed = false;
	bool bPendingStale = false;
	bool bPendingLookahead = false;

	void ApplyLookStep(ADLPlayerCharacter* Char, const FDLAgentStep& Step) const;
};

/** Cursor MCP connector: same motor as remoteAgent, hub snapshots only when stale. */
UCLASS()
class DESTINYLIKE_API UDLCursorPlaybook : public UDLRemoteAgentPlaybook
{
	GENERATED_BODY()

public:
	virtual FString GetKindId() const override { return TEXT("cursor"); }
	virtual bool WantsHubSnapshot(EDLHubSnapshotReason Reason) const override;
};
