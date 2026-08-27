#include "Player/CLHeadlessAgent.h"
#include "Player/CLPossessionComponent.h"

ACLHeadlessAgent::ACLHeadlessAgent()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	Possession = CreateDefaultSubobject<UCLPossessionComponent>(TEXT("Possession"));
	Possession->GoHeadless();
}
