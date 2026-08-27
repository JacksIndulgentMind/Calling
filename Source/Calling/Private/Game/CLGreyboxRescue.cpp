#include "Game/CLGreyboxRescue.h"
#include "Game/CLGreyboxFloors.h"
#include "Game/CLGameModeBase.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UCLGreyboxRescue::UCLGreyboxRescue()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCLGreyboxRescue::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RescueFallenPawns();
	RespawnMissingPawns(DeltaTime);
}

void UCLGreyboxRescue::RescueFallenPawns() const
{
	UWorld* World = GetWorld();
	ACLGameModeBase* GM = World ? World->GetAuthGameMode<ACLGameModeBase>() : nullptr;
	const ACLGreyboxFloors* Floors = Cast<ACLGreyboxFloors>(GetOwner());
	if (!GM || !Floors)
	{
		return;
	}

	const float MinZ = Floors->GetRescueMinZ();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn && Pawn->GetActorLocation().Z < MinZ)
		{
			GM->RequestRespawn(PC);
		}
	}
}

void UCLGreyboxRescue::RespawnMissingPawns(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	ACLGameModeBase* GM = World ? World->GetAuthGameMode<ACLGameModeBase>() : nullptr;
	if (!GM)
	{
		return;
	}

	bool bMissing = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->GetPawn())
		{
			bMissing = true;
			break;
		}
	}
	if (!bMissing)
	{
		MissingPawnSeconds = 0.f;
		return;
	}
	MissingPawnSeconds += DeltaSeconds;
	if (MissingPawnSeconds < 0.2f)
	{
		return;
	}
	MissingPawnSeconds = 0.f;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (!PC->GetPawn())
			{
				GM->RequestRespawn(PC);
			}
		}
	}
}
