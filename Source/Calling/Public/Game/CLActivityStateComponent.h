#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CLTypes.h"
#include "CLActivityStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCLOnActivityPhaseChanged, ECLActivityPhase, OldPhase, ECLActivityPhase, NewPhase);

/**
 * Scene-level FSM shared by Social / PvP / Raid / Practice game modes.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLActivityStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLActivityStateComponent();

	UFUNCTION(BlueprintCallable, Category = "Calling|Activity")
	void SetPhase(ECLActivityPhase NewPhase);

	UFUNCTION(BlueprintPure, Category = "Calling|Activity")
	ECLActivityPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintCallable, Category = "Calling|Activity")
	void BeginLobby();

	UFUNCTION(BlueprintCallable, Category = "Calling|Activity")
	void BeginLoading();

	UFUNCTION(BlueprintCallable, Category = "Calling|Activity")
	void BeginInProgress();

	UFUNCTION(BlueprintCallable, Category = "Calling|Activity")
	void BeginResults();

	UFUNCTION(BlueprintCallable, Category = "Calling|Activity")
	void BeginReturning();

	UPROPERTY(BlueprintAssignable, Category = "Calling|Activity")
	FCLOnActivityPhaseChanged OnPhaseChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "Calling|Activity")
	ECLActivityPhase Phase = ECLActivityPhase::Lobby;

	UFUNCTION()
	void OnRep_Phase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	ECLActivityPhase PreviousPhase = ECLActivityPhase::Lobby;
};
