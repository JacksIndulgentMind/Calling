#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DLTunes.h"
#include "DLTickClock.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FDLOnFixedGameTick, float /*DeltaSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDLOnNetTick, float /*DeltaSeconds*/);

/**
 * Fixed game-sim clock (default 30Hz) + net clock (default 20Hz).
 * Render runs unbounded; consumers interpolate from pose samples.
 */
UCLASS()
class DESTINYLIKE_API UDLTickSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableInEditor() const override { return false; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Tick")
	float GetGameSimHz() const { return Tune.GameSimHz; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Tick")
	float GetNetHz() const { return Tune.NetHz; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Tick")
	float GetGameSimAlpha() const { return GameSimAlpha; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Tick")
	int32 GetGameTickIndex() const { return GameTickIndex; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Tick")
	int32 GetNetTickIndex() const { return NetTickIndex; }

	FDLOnFixedGameTick& OnFixedGameTick() { return FixedGameTickEvent; }
	FDLOnNetTick& OnNetTick() { return NetTickEvent; }

	const FDLTickTune& GetTune() const { return Tune; }

	void ReloadSettingsFromConfig();

private:
	UPROPERTY()
	FDLTickTune Tune;

	float GameAccumulator = 0.f;
	float NetAccumulator = 0.f;
	float GameSimAlpha = 0.f;
	int32 GameTickIndex = 0;
	int32 NetTickIndex = 0;

	FDLOnFixedGameTick FixedGameTickEvent;
	FDLOnNetTick NetTickEvent;
};

/** Non-UObject helper for units that prefer value semantics. */
struct DESTINYLIKE_API FDLTickClock
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
