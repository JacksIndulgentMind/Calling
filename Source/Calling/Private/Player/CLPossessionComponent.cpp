#include "Player/CLPossessionComponent.h"
#include "GameFramework/Pawn.h"

UCLPossessionComponent::UCLPossessionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APawn* UCLPossessionComponent::GetDrivenPawn() const
{
	return DrivenPawn.Get();
}

void UCLPossessionComponent::PossessOwn(APawn* Pawn)
{
	OwnPawn = Pawn;
	DrivenPawn = Pawn;
	Mode = Pawn ? ECLPossessionMode::OwnPawn : ECLPossessionMode::Headless;
}

void UCLPossessionComponent::MindControl(APawn* Pawn)
{
	DrivenPawn = Pawn;
	Mode = Pawn ? ECLPossessionMode::MindControl : ECLPossessionMode::Headless;
}

void UCLPossessionComponent::GoHeadless()
{
	DrivenPawn = nullptr;
	Mode = ECLPossessionMode::Headless;
}

bool UCLPossessionComponent::Drives(const APawn* Pawn) const
{
	return Pawn != nullptr && DrivenPawn.Get() == Pawn;
}
