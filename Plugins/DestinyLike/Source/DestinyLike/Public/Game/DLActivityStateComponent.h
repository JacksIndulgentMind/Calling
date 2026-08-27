#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DLTypes.h"
#include "DLActivityStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLOnActivityPhaseChanged, EDLActivityPhase, OldPhase, EDLActivityPhase, NewPhase);

/**
 * Scene-level FSM shared by Social / PvP / Raid / Practice game modes.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLActivityStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLActivityStateComponent();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Activity")
	void SetPhase(EDLActivityPhase NewPhase);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Activity")
	EDLActivityPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Activity")
	void BeginLobby();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Activity")
	void BeginLoading();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Activity")
	void BeginInProgress();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Activity")
	void BeginResults();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Activity")
	void BeginReturning();

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Activity")
	FDLOnActivityPhaseChanged OnPhaseChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "DestinyLike|Activity")
	EDLActivityPhase Phase = EDLActivityPhase::Lobby;

	UFUNCTION()
	void OnRep_Phase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	EDLActivityPhase PreviousPhase = EDLActivityPhase::Lobby;
};
