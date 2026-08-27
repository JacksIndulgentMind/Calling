#include "Player/DLPossessionComponent.h"
#include "GameFramework/Pawn.h"

UDLPossessionComponent::UDLPossessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APawn* UDLPossessionComponent::GetDrivenPawn() const
{
	return DrivenPawn.Get();
}

void UDLPossessionComponent::PossessOwn(APawn* Pawn)
{
	OwnPawn = Pawn;
	DrivenPawn = Pawn;
	Mode = Pawn ? EDLPossessionMode::OwnPawn : EDLPossessionMode::Headless;
}

void UDLPossessionComponent::MindControl(APawn* Pawn)
{
	DrivenPawn = Pawn;
	Mode = Pawn ? EDLPossessionMode::MindControl : EDLPossessionMode::Headless;
}

void UDLPossessionComponent::GoHeadless()
{
	DrivenPawn = nullptr;
	Mode = EDLPossessionMode::Headless;
}

bool UDLPossessionComponent::Drives(const APawn* Pawn) const
{
	return Pawn != nullptr && DrivenPawn.Get() == Pawn;
}
