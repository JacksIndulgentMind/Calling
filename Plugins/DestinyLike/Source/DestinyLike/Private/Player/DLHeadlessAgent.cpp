#include "Player/DLHeadlessAgent.h"
#include "Player/DLPossessionComponent.h"

ADLHeadlessAgent::ADLHeadlessAgent()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	Possession = CreateDefaultSubobject<UDLPossessionComponent>(TEXT("Possession"));
	Possession->GoHeadless();
}
