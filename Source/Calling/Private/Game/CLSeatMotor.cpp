#include "Game/CLSeatMotor.h"
#include "Game/CLParticipantSeat.h"
#include "AI/CLBotBookManager.h"
#include "Player/CLPlayerCharacter.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void UCLSeatMotor::TickNet(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	(void)DeltaSeconds;
	(void)Seat;
}

bool UCLSeatMotor::WantsHubSnapshot(ECLHubSnapshotReason Reason) const
{
	(void)Reason;
	return false;
}

bool UCLSeatMotor::StartGoto(UWorld* World, ACLPlayerCharacter* Char, const FVector& Dest, FString& OutError)
{
	return Goto.Start(World, Char, Dest, OutError, false);
}

void UCLSeatMotor::CancelGoto()
{
	Goto.Cancel();
}

bool UCLRemoteAgentSeatMotor::TickBotBook(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	UWorld* World = Char ? Char->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UCLBotBookManager* Mgr = GI ? GI->GetSubsystem<UCLBotBookManager>() : nullptr;
	if (!Mgr)
	{
		return false;
	}
	return Mgr->TickSeat(DeltaSeconds, Seat);
}

void UCLAlgorithmicSeatMotor::TickNet(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	UWorld* World = Char ? Char->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UCLBotBookManager* Mgr = GI ? GI->GetSubsystem<UCLBotBookManager>() : nullptr)
	{
		Mgr->TickSeat(DeltaSeconds, Seat);
	}
}

void UCLRemoteAgentSeatMotor::ApplyLookStep(ACLPlayerCharacter* Char, const FCLAgentStep& Step) const
{
	if (!Char)
	{
		return;
	}
	Char->ApplyAgentLookFromStep(Step.TrackSeatId, Step.Look);
}

void UCLRemoteAgentSeatMotor::TickNet(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	if (!Char)
	{
		return;
	}

	if (TickBotBook(DeltaSeconds, Seat))
	{
		bNeedsReplan = false;
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

bool UCLRemoteAgentSeatMotor::WantsHubSnapshot(ECLHubSnapshotReason Reason) const
{
	(void)Reason;
	return true;
}

bool UCLCursorSeatMotor::WantsHubSnapshot(ECLHubSnapshotReason Reason) const
{
	return Reason == ECLHubSnapshotReason::Stale;
}

bool UCLRemoteAgentSeatMotor::ConsumePendingSnapshot(ECLHubSnapshotReason& OutReason)
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

bool UCLRemoteAgentSeatMotor::QueuePlan(const TArray<FCLAgentStep>& Steps, bool bRemainder, FString& OutError)
{
	Goto.Cancel();
	MarkReply();
	bNeedsReplan = false;
	return Plan.Queue(Steps, bRemainder, OutError);
}

void UCLRemoteAgentSeatMotor::MarkReply()
{
	LastReplySeconds = FPlatformTime::Seconds();
	bNeedsReplan = false;
	bLookaheadPushed = false;
	bPendingStale = false;
	bPendingLookahead = false;
}

void UCLRemoteAgentSeatMotor::CancelPlan()
{
	Plan.Cancel();
}

void UCLRemoteAgentSeatMotor::CancelMotor()
{
	CancelPlan();
	CancelGoto();
}
