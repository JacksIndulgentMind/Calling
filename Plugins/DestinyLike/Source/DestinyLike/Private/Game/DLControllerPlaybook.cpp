#include "Game/DLControllerPlaybook.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLAgentGotoDriver.h"
#include "Player/DLPlayerCharacter.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"

void UDLControllerPlaybook::TickNet(float DeltaSeconds, UDLParticipantSeat* Seat)
{
	(void)DeltaSeconds;
	(void)Seat;
}

bool UDLControllerPlaybook::WantsHubSnapshot(EDLHubSnapshotReason Reason) const
{
	(void)Reason;
	return false;
}

void UDLRemoteAgentPlaybook::ApplyLookStep(ADLPlayerCharacter* Char, const FDLAgentStep& Step) const
{
	if (!Char)
	{
		return;
	}
	Char->ApplyAgentLookFromStep(Step.TrackSeatId, Step.Look);
}

void UDLRemoteAgentPlaybook::TickNet(float DeltaSeconds, UDLParticipantSeat* Seat)
{
	ADLPlayerCharacter* Char = Seat ? Cast<ADLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
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
			[this](ADLPlayerCharacter* C, const FDLAgentStep& Step)
			{
				ApplyLookStep(C, Step);
				C->ApplyAgentIntent(Step.ToIntent(true));
			},
			[this](ADLPlayerCharacter* C, const FDLAgentStep& Step)
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

bool UDLRemoteAgentPlaybook::WantsHubSnapshot(EDLHubSnapshotReason Reason) const
{
	(void)Reason;
	return true;
}

bool UDLCursorPlaybook::WantsHubSnapshot(EDLHubSnapshotReason Reason) const
{
	return Reason == EDLHubSnapshotReason::Stale;
}

bool UDLRemoteAgentPlaybook::ConsumePendingSnapshot(EDLHubSnapshotReason& OutReason)
{
	if (bPendingStale)
	{
		bPendingStale = false;
		OutReason = EDLHubSnapshotReason::Stale;
		return true;
	}
	if (bPendingLookahead)
	{
		bPendingLookahead = false;
		OutReason = EDLHubSnapshotReason::LowLookahead;
		return true;
	}
	return false;
}

bool UDLRemoteAgentPlaybook::QueuePlan(const TArray<FDLAgentStep>& Steps, bool bRemainder, FString& OutError)
{
	Goto.Cancel();
	MarkReply();
	bNeedsReplan = false;
	return Plan.Queue(Steps, bRemainder, OutError);
}

bool UDLRemoteAgentPlaybook::StartGoto(UWorld* World, ADLPlayerCharacter* Char, const FVector& Dest, FString& OutError)
{
	CancelPlan();
	MarkReply();
	return Goto.Start(World, Char, Dest, OutError, false);
}

void UDLRemoteAgentPlaybook::MarkReply()
{
	LastReplySeconds = FPlatformTime::Seconds();
	bNeedsReplan = false;
	bLookaheadPushed = false;
	bPendingStale = false;
	bPendingLookahead = false;
}

void UDLRemoteAgentPlaybook::CancelPlan()
{
	Plan.Cancel();
}

void UDLRemoteAgentPlaybook::CancelGoto()
{
	Goto.Cancel();
}
