#include "Game/DLActivityStateComponent.h"
#include "Net/UnrealNetwork.h"

UDLActivityStateComponent::UDLActivityStateComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UDLActivityStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDLActivityStateComponent, Phase);
}

void UDLActivityStateComponent::SetPhase(EDLActivityPhase NewPhase)
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

void UDLActivityStateComponent::OnRep_Phase()
{
	OnPhaseChanged.Broadcast(PreviousPhase, Phase);
	PreviousPhase = Phase;
}

void UDLActivityStateComponent::BeginLobby() { SetPhase(EDLActivityPhase::Lobby); }
void UDLActivityStateComponent::BeginLoading() { SetPhase(EDLActivityPhase::Loading); }
void UDLActivityStateComponent::BeginInProgress() { SetPhase(EDLActivityPhase::InProgress); }
void UDLActivityStateComponent::BeginResults() { SetPhase(EDLActivityPhase::Results); }
void UDLActivityStateComponent::BeginReturning() { SetPhase(EDLActivityPhase::Returning); }
