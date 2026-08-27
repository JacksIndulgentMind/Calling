#include "Game/CLAgentSequenceRunner.h"
#include "Player/CLPlayerCharacter.h"

namespace
{
	constexpr int32 MaxSequenceSteps = 32;
	constexpr float MaxSequenceSeconds = 60.f;
}

float FCLAgentSequenceRunner::RemainingSeconds() const
{
	if (!Steps.IsValidIndex(Index))
	{
		return 0.f;
	}
	float Remaining = TimeLeft;
	for (int32 i = Index + 1; i < Steps.Num(); ++i)
	{
		Remaining += Steps[i].Seconds;
	}
	return Remaining;
}

void FCLAgentSequenceRunner::Cancel()
{
	Steps.Reset();
	Index = 0;
	TimeLeft = 0.f;
	bPulsesSent = false;
}

bool FCLAgentSequenceRunner::Queue(const TArray<FCLAgentStep>& NewSteps, bool bAfterCurrent, FString& OutError)
{
	(void)OutError;
	TArray<FCLAgentStep> Clamped;
	float Total = bAfterCurrent && Steps.IsValidIndex(Index) ? TimeLeft : 0.f;
	const int32 Room = bAfterCurrent ? (MaxSequenceSteps - 1) : MaxSequenceSteps;
	for (const FCLAgentStep& Step : NewSteps)
	{
		if (Clamped.Num() >= Room)
		{
			break;
		}
		FCLAgentStep Copy = Step;
		if (Copy.Seconds <= KINDA_SMALL_NUMBER)
		{
			Copy.Seconds = 1.f / 30.f;
		}
		if (Total + Copy.Seconds > MaxSequenceSeconds)
		{
			Copy.Seconds = FMath::Max(0.f, MaxSequenceSeconds - Total);
			if (Copy.Seconds <= KINDA_SMALL_NUMBER)
			{
				break;
			}
			Clamped.Add(Copy);
			break;
		}
		Total += Copy.Seconds;
		Clamped.Add(Copy);
	}

	if (bAfterCurrent && Steps.IsValidIndex(Index))
	{
		TArray<FCLAgentStep> Kept;
		Kept.Add(Steps[Index]);
		Kept.Append(Clamped);
		Steps = MoveTemp(Kept);
		Index = 0;
	}
	else
	{
		Steps = MoveTemp(Clamped);
		Index = 0;
		TimeLeft = 0.f;
		bPulsesSent = false;
	}
	return true;
}

void FCLAgentSequenceRunner::Tick(float DeltaSeconds, ACLPlayerCharacter* Char,
	TFunctionRef<void(ACLPlayerCharacter*, const FCLAgentStep&)> ApplyPulses,
	TFunctionRef<void(ACLPlayerCharacter*, const FCLAgentStep&)> ApplyHolds)
{
	if (!Char || !Steps.IsValidIndex(Index))
	{
		return;
	}

	if (!bPulsesSent)
	{
		const FCLAgentStep& Step = Steps[Index];
		TimeLeft = Step.Seconds;
		ApplyPulses(Char, Step);
		bPulsesSent = true;
		return;
	}

	ApplyHolds(Char, Steps[Index]);
	TimeLeft -= DeltaSeconds;
	if (TimeLeft > 0.f)
	{
		return;
	}

	++Index;
	bPulsesSent = false;
	TimeLeft = 0.f;
	if (!Steps.IsValidIndex(Index))
	{
		Cancel();
		Char->ClearAgentIntent();
	}
}
