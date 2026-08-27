#include "Game/CLActivityStateComponent.h"
#include "Net/UnrealNetwork.h"

UCLActivityStateComponent::UCLActivityStateComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UCLActivityStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCLActivityStateComponent, Phase);
}

void UCLActivityStateComponent::SetPhase(ECLActivityPhase NewPhase)
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		return;
	}
	if (Phase == NewPhase)
	{
		return;
	}

	PreviousPhase = Phase;
	Phase = NewPhase;
	OnPhaseChanged.Broadcast(PreviousPhase, Phase);
}

void UCLActivityStateComponent::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(PreviousPhase, Phase);
	PreviousPhase = Phase;
}

void UCLActivityStateComponent::BeginLobby() { SetPhase(ECLActivityPhase::Lobby); }
void UCLActivityStateComponent::BeginLoading() { SetPhase(ECLActivityPhase::Loading); }
void UCLActivityStateComponent::BeginInProgress() { SetPhase(ECLActivityPhase::InProgress); }
void UCLActivityStateComponent::BeginResults() { SetPhase(ECLActivityPhase::Results); }
void UCLActivityStateComponent::BeginReturning() { SetPhase(ECLActivityPhase::Returning); }
