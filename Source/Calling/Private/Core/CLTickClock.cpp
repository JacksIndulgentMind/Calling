#include "Core/CLTickClock.h"
#include "Misc/ConfigCacheIni.h"

void UCLTickSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadSettingsFromConfig();
}

void UCLTickSubsystem::Deinitialize()
{
	FixedGameTickEvent.Clear();
	NetTickEvent.Clear();
	Super::Deinitialize();
}

void UCLTickSubsystem::ReloadSettingsFromConfig()
{
	Tune.LoadFromIni();
}

void UCLTickSubsystem::Tick(float DeltaTime)
{
	if (Tune.GameSimHz <= KINDA_SMALL_NUMBER || Tune.NetHz <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float GameStep = 1.f / Tune.GameSimHz;
	const float NetStep = 1.f / Tune.NetHz;

	if (Tune.bLockGameSimToFixedStep)
	{
		GameAccumulator += DeltaTime;
		// Spiral-of-death guard
		constexpr int32 MaxSteps = 8;
		int32 Steps = 0;
		while (GameAccumulator >= GameStep && Steps < MaxSteps)
		{
			FixedGameTickEvent.Broadcast(GameStep);
			++GameTickIndex;
			GameAccumulator -= GameStep;
			++Steps;
		}
		GameSimAlpha = FMath::Clamp(GameAccumulator / GameStep, 0.f, 1.f);
	}
	else
	{
		FixedGameTickEvent.Broadcast(DeltaTime);
		++GameTickIndex;
		GameSimAlpha = 1.f;
	}

	NetAccumulator += DeltaTime;
	constexpr int32 MaxNetSteps = 4;
	int32 NetSteps = 0;
	while (NetAccumulator >= NetStep && NetSteps < MaxNetSteps)
	{
		NetTickEvent.Broadcast(NetStep);
		++NetTickIndex;
		NetAccumulator -= NetStep;
		++NetSteps;
	}
}

TStatId UCLTickSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCLTickSubsystem, STATGROUP_Tickables);
}

void FCLTickClock::Advance(float DeltaSeconds, TFunctionRef<void(float)> OnGame, TFunctionRef<void(float)> OnNet)
{
	const float GameStep = 1.f / FMath::Max(GameSimHz, 1.f);
	const float NetStep = 1.f / FMath::Max(NetHz, 1.f);

	GameAccumulator += DeltaSeconds;
	int32 Steps = 0;
	while (GameAccumulator >= GameStep && Steps < 8)
	{
		OnGame(GameStep);
		++GameTick;
		GameAccumulator -= GameStep;
		++Steps;
	}
	Alpha = FMath::Clamp(GameAccumulator / GameStep, 0.f, 1.f);

	NetAccumulator += DeltaSeconds;
	int32 NetSteps = 0;
	while (NetAccumulator >= NetStep && NetSteps < 4)
	{
		OnNet(NetStep);
		++NetTick;
		NetAccumulator -= NetStep;
		++NetSteps;
	}
}
