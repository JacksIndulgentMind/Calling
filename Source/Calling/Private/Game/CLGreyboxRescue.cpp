#include "Game/CLGreyboxRescue.h"
#include "Game/CLGreyboxFloors.h"
#include "Game/CLGameModeBase.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	RecallEdgePad(DeltaTime);
	RespawnMissingPawns(DeltaTime);
}

void UCLGreyboxRescue::TeleportToLip(ACLPlayerCharacter* Char, const FVector& LipStand)
{
	if (!Char)
	{
		return;
	}
	Char->TeleportTo(LipStand, Char->GetActorRotation(), false, true);
	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	Char->ClearAgentIntent();
}

void UCLGreyboxRescue::RescueFallenPawns() const
{
	UWorld* World = GetWorld();
	const ACLGreyboxFloors* Floors = Cast<ACLGreyboxFloors>(GetOwner());
	if (!World || !Floors)
	{
		return;
	}

	const float MinZ = Floors->GetRescueMinZ();
	const FVector Lip = Floors->GetEdgeRecallLocation();
	for (TActorIterator<ACLPlayerCharacter> It(World); It; ++It)
	{
		ACLPlayerCharacter* Char = *It;
		if (Char && Char->GetActorLocation().Z < MinZ)
		{
			TeleportToLip(Char, Lip);
		}
	}
}

void UCLGreyboxRescue::RecallEdgePad(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	ACLGreyboxFloors* Floors = Cast<ACLGreyboxFloors>(GetOwner());
	if (!World || !Floors || !Floors->HasEdgePad())
	{
		return;
	}

	const FVector Lip = Floors->GetEdgeRecallLocation();
	TSet<uint32> Seen;
	for (TActorIterator<ACLPlayerCharacter> It(World); It; ++It)
	{
		ACLPlayerCharacter* Char = *It;
		if (!Char)
		{
			continue;
		}
		const uint32 Id = Char->GetUniqueID();
		Seen.Add(Id);
		const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
		const bool bOnFloor = Move && Move->IsMovingOnGround() && !Move->IsDiving();
		if (bOnFloor && Floors->IsOnEdgePad(Char->GetActorLocation()))
		{
			float& Stand = PadStandSeconds.FindOrAdd(Id);
			Stand += DeltaSeconds;
			if (Stand >= 0.45f)
			{
				TeleportToLip(Char, Lip);
				PadStandSeconds.Remove(Id);
			}
		}
		else
		{
			PadStandSeconds.Remove(Id);
		}
	}
	for (auto It = PadStandSeconds.CreateIterator(); It; ++It)
	{
		if (!Seen.Contains(It->Key))
		{
			It.RemoveCurrent();
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
