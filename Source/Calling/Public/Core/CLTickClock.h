#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CLTunes.h"
#include "CLTickClock.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FCLOnFixedGameTick, float /*DeltaSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FCLOnNetTick, float /*DeltaSeconds*/);

/**
 * Fixed game-sim clock (default 30Hz) + net clock (default 20Hz).
 * Render runs unbounded; consumers interpolate from pose samples.
 */
UCLASS()
class CALLING_API UCLTickSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableInEditor() const override { return false; }

	UFUNCTION(BlueprintPure, Category = "Calling|Tick")
	float GetGameSimHz() const { return Tune.GameSimHz; }

	UFUNCTION(BlueprintPure, Category = "Calling|Tick")
	float GetNetHz() const { return Tune.NetHz; }

	UFUNCTION(BlueprintPure, Category = "Calling|Tick")
	float GetGameSimAlpha() const { return GameSimAlpha; }

	UFUNCTION(BlueprintPure, Category = "Calling|Tick")
	int32 GetGameTickIndex() const { return GameTickIndex; }

	UFUNCTION(BlueprintPure, Category = "Calling|Tick")
	int32 GetNetTickIndex() const { return NetTickIndex; }

	FCLOnFixedGameTick& OnFixedGameTick() { return FixedGameTickEvent; }
	FCLOnNetTick& OnNetTick() { return NetTickEvent; }

	const FCLTickTune& GetTune() const { return Tune; }

	void ReloadSettingsFromConfig();

private:
	UPROPERTY()
	FCLTickTune Tune;

	float GameAccumulator = 0.f;
	float NetAccumulator = 0.f;
	float GameSimAlpha = 0.f;
	int32 GameTickIndex = 0;
	int32 NetTickIndex = 0;

	FCLOnFixedGameTick FixedGameTickEvent;
	FCLOnNetTick NetTickEvent;
};

/** Non-UObject helper for units that prefer value semantics. */
struct CALLING_API FCLTickClock
{
	float GameSimHz = 30.f;
	float NetHz = 20.f;
	float GameAccumulator = 0.f;
	float NetAccumulator = 0.f;
	float Alpha = 0.f;
	int32 GameTick = 0;
	int32 NetTick = 0;

	void Advance(float DeltaSeconds, TFunctionRef<void(float)> OnGame, TFunctionRef<void(float)> OnNet);
};
