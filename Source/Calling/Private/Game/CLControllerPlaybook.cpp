#include "Game/CLControllerPlaybook.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLAgentGotoDriver.h"
#include "Player/CLPlayerCharacter.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"

void UCLControllerPlaybook::TickNet(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	(void)DeltaSeconds;
	(void)Seat;
}

bool UCLControllerPlaybook::WantsHubSnapshot(ECLHubSnapshotReason Reason) const
{
	(void)Reason;
	return false;
}

void UCLRemoteAgentPlaybook::ApplyLookStep(ACLPlayerCharacter* Char, const FCLAgentStep& Step) const
{
	if (!Char)
	{
		return;
	}
	Char->ApplyAgentLookFromStep(Step.TrackSeatId, Step.Look);
}

void UCLRemoteAgentPlaybook::TickNet(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	if (!Char)
	{
		return;
	}

	if (Goto.bActive)
	{
		Goto.Tick(DeltaSeconds, Char->GetWorld(), Char);
		if (Goto.bActive)
		{
			bNeedsReplan = false;
			return;
		}
	}

	const double Now = FPlatformTime::Seconds();
	if (Plan.IsActive())
	{
		Plan.Tick(DeltaSeconds, Char,
			[this](ACLPlayerCharacter* C, const FCLAgentStep& Step)
			{
				ApplyLookStep(C, Step);
				C->ApplyAgentIntent(Step.ToIntent(true));
			},
			[this](ACLPlayerCharacter* C, const FCLAgentStep& Step)
			{
				ApplyLookStep(C, Step);
				C->ApplyAgentIntent(Step.ToIntent(false));
			});
		bNeedsReplan = false;
		if (!bLookaheadPushed && Plan.RemainingSeconds() <= LookaheadSeconds)
		{
			bLookaheadPushed = true;
			bPendingLookahead = true;
		}
		return;
	}

	if (LastReplySeconds > 0.0 && (Now - LastReplySeconds) >= StaleSeconds)
	{
		if (!bNeedsReplan)
		{
			bPendingStale = true;
		}
		bNeedsReplan = true;
	}
}

bool UCLRemoteAgentPlaybook::WantsHubSnapshot(ECLHubSnapshotReason Reason) const
{
	(void)Reason;
	return true;
}

bool UCLCursorPlaybook::WantsHubSnapshot(ECLHubSnapshotReason Reason) const
{
	return Reason == ECLHubSnapshotReason::Stale;
}

bool UCLRemoteAgentPlaybook::ConsumePendingSnapshot(ECLHubSnapshotReason& OutReason)
{
	if (bPendingStale)
	{
		bPendingStale = false;
		OutReason = ECLHubSnapshotReason::Stale;
		return true;
	}
	if (bPendingLookahead)
	{
		bPendingLookahead = false;
		OutReason = ECLHubSnapshotReason::LowLookahead;
		return true;
	}
	return false;
}

bool UCLRemoteAgentPlaybook::QueuePlan(const TArray<FCLAgentStep>& Steps, bool bRemainder, FString& OutError)
{
	Goto.Cancel();
	MarkReply();
	bNeedsReplan = false;
	return Plan.Queue(Steps, bRemainder, OutError);
}

bool UCLRemoteAgentPlaybook::StartGoto(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError)
{
	CancelPlan();
	MarkReply();
	return Goto.Start(World, Char, Dest, OutError, false);
}

void UCLRemoteAgentPlaybook::MarkReply()
{
	LastReplySeconds = FPlatformTime::Seconds();
	bNeedsReplan = false;
	bLookaheadPushed = false;
	bPendingStale = false;
	bPendingLookahead = false;
}

void UCLRemoteAgentPlaybook::CancelPlan()
{
	Plan.Cancel();
}

void UCLRemoteAgentPlaybook::CancelGoto()
{
	Goto.Cancel();
}

void UCLRemoteAgentPlaybook::CancelMotor()
{
	CancelPlan();
	CancelGoto();
}
