#include "Game/CLGateCountdown.h"
#include "Game/CLLobbyTypes.h"
#include "Misc/ConfigCacheIni.h"

void UCLGateCountdown::ResetOpen()
{
	Gate = nullptr;
	bGameplayUnlocked = true;
	bCountdownRunning = false;
	bMatchStartQueued = false;
	CountdownRemaining = 0.f;
}

void UCLGateCountdown::ResetLocked()
{
	bGameplayUnlocked = false;
	bCountdownRunning = false;
	bMatchStartQueued = false;
	CountdownRemaining = 0.f;
}

void UCLGateCountdown::InstallFromConfig()
{
	Gate = NewObject<UCLGateBox>(this);
	float Countdown = 1.f;
	float Stale = 3.f;
	GConfig->GetFloat(TEXT("/Script/Calling.CLLobbySettings"), TEXT("CountdownSeconds"), Countdown, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLLobbySettings"), TEXT("PlanStaleSeconds"), Stale, GGameIni);
	float Lookahead = 0.75f;
	GConfig->GetFloat(TEXT("/Script/Calling.CLLobbySettings"), TEXT("PlanLookaheadSeconds"), Lookahead, GGameIni);
	Gate->Value.CountdownSeconds = FMath::Max(0.2f, Countdown);
	Gate->Value.PlanStaleSeconds = FMath::Max(0.25f, Stale);
	Gate->Value.PlanLookaheadSeconds = FMath::Max(0.1f, Lookahead);
}

void UCLGateCountdown::LoadLaunchSecondsFromConfig()
{
	LaunchCountdownSeconds = 1.f;
	GConfig->GetFloat(TEXT("/Script/Calling.CLLobbySettings"), TEXT("CountdownSeconds"), LaunchCountdownSeconds, GGameIni);
	LaunchCountdownSeconds = FMath::Max(0.2f, LaunchCountdownSeconds);
}

void UCLGateCountdown::ClearGate()
{
	Gate = nullptr;
}

const FCLLobbyGate* UCLGateCountdown::GetGate() const
{
	return Gate ? &Gate->Value : nullptr;
}

void UCLGateCountdown::StartCountdownIfReady(const FCLLobbyInvoice* Invoice, int32 ReadyCount)
{
	const FCLLobbyGate* LiveGate = GetGate();
	if (!Invoice || !LiveGate || bGameplayUnlocked || bMatchStartQueued)
	{
		return;
	}
	if (ReadyCount < Invoice->MinPlayers)
	{
		bCountdownRunning = false;
		CountdownRemaining = 0.f;
		return;
	}
	if (!bCountdownRunning)
	{
		bCountdownRunning = true;
		CountdownRemaining = LiveGate->CountdownSeconds;
	}
}

bool UCLGateCountdown::RequestGo(const FCLLobbyInvoice* Invoice, int32 ReadyCount, bool bHostOk, TFunction<void()> OnFinishGo)
{
	if (!bHostOk)
	{
		return false;
	}
	if (!Gate)
	{
		if (!Invoice || ReadyCount < Invoice->MinPlayers)
		{
			return false;
		}
		bMatchStartQueued = true;
		if (!bCountdownRunning)
		{
			bCountdownRunning = true;
			CountdownRemaining = LaunchCountdownSeconds;
			return true;
		}
		if (CountdownRemaining > 0.f)
		{
			return true;
		}
		FinishGo(MoveTemp(OnFinishGo));
		return true;
	}
	if (!Invoice || ReadyCount < Invoice->MinPlayers)
	{
		return false;
	}
	bMatchStartQueued = true;
	FinishGo(MoveTemp(OnFinishGo));
	return true;
}

void UCLGateCountdown::TickCountdown(float DeltaSeconds, TFunction<void()> OnFinishGo)
{
	if (!bCountdownRunning)
	{
		return;
	}
	CountdownRemaining -= DeltaSeconds;
	if (CountdownRemaining <= 0.f)
	{
		bCountdownRunning = false;
		CountdownRemaining = 0.f;
		if (bMatchStartQueued || Gate)
		{
			FinishGo(MoveTemp(OnFinishGo));
		}
	}
}

void UCLGateCountdown::CancelCountdownIfUnready()
{
	if (!bMatchStartQueued)
	{
		bCountdownRunning = false;
		CountdownRemaining = 0.f;
	}
}

void UCLGateCountdown::FinishGo(TFunction<void()> OnUnlocked)
{
	bCountdownRunning = false;
	CountdownRemaining = 0.f;
	bGameplayUnlocked = true;
	if (OnUnlocked)
	{
		OnUnlocked();
	}
}
