#include "Game/CLSeatRegistry.h"
#include "Game/CLLobbyTypes.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSeatMotor.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatPawn.h"
#include "Player/CLPossessionComponent.h"
#include "Player/CLHeadlessAgent.h"
#include "AI/CLSeatController.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/PlayerStartPIE.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	UWorld* WorldFromOuter(const UObject* Obj)
	{
		const UObject* Outer = Obj ? Obj->GetOuter() : nullptr;
		return Outer ? Outer->GetWorld() : nullptr;
	}
}

void UCLSeatRegistry::Reset()
{
	Seats.Reset();
}

TArray<UCLParticipantSeat*> UCLSeatRegistry::GetAll() const
{
	TArray<UCLParticipantSeat*> Out;
	for (const TObjectPtr<UCLParticipantSeat>& Seat : Seats)
	{
		if (Seat)
		{
			Out.Add(Seat.Get());
		}
	}
	return Out;
}

int32 UCLSeatRegistry::Num() const
{
	return Seats.Num();
}

UCLParticipantSeat* UCLSeatRegistry::Find(const FGuid& SeatId) const
{
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetSeatId() == SeatId)
		{
			return Seat;
		}
	}
	return nullptr;
}

UCLParticipantSeat* UCLSeatRegistry::FindByName(const FString& DisplayName) const
{
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetDisplayName().Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			return Seat;
		}
	}
	return nullptr;
}

UCLParticipantSeat* UCLSeatRegistry::FindHost() const
{
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->IsHost())
		{
			return Seat;
		}
	}
	return nullptr;
}

UCLParticipantSeat* UCLSeatRegistry::FindLocal() const
{
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetBoundController() && Seat->GetBoundController()->IsLocalController())
		{
			return Seat;
		}
	}
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetSeatMotor() && Seat->GetSeatMotor()->IsA<UCLHumanSeatMotor>())
		{
			if (const APawn* Pawn = Seat->GetDrivenPawn())
			{
				if (Pawn->IsLocallyControlled())
				{
					return Seat;
				}
			}
		}
	}
	return FindHost();
}

UCLParticipantSeat* UCLSeatRegistry::FindForController(const AController* Controller) const
{
	if (!Controller)
	{
		return nullptr;
	}
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetBoundController() == Controller)
		{
			return Seat;
		}
	}
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (PC->IsLocalController())
		{
			if (UCLParticipantSeat* Local = FindLocal())
			{
				return Local;
			}
		}
	}
	const APawn* Pawn = Controller->GetPawn();
	for (UCLParticipantSeat* Seat : Seats)
	{
		if (!Seat)
		{
			continue;
		}
		if (Seat->GetDrivenPawn() == Pawn || Seat->GetAnchor() == Controller)
		{
			return Seat;
		}
	}
	return nullptr;
}

APawn* UCLSeatRegistry::GetDrivenPawn(const FGuid& SeatId) const
{
	const UCLParticipantSeat* Seat = Find(SeatId);
	return Seat ? Seat->GetDrivenPawn() : nullptr;
}

bool UCLSeatRegistry::IsRemotelyDriven(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	for (const UCLParticipantSeat* Seat : Seats)
	{
		if (!Seat || !Seat->GetPossession() || !Seat->GetPossession()->Drives(Pawn))
		{
			continue;
		}
		if (Seat->GetSeatMotor() && Seat->GetSeatMotor()->IsA<UCLRemoteAgentSeatMotor>())
		{
			return Seat->GetPossession()->GetMode() == ECLPossessionMode::MindControl
				|| !Pawn->IsLocallyControlled();
		}
	}
	return false;
}

ACLPlayerCharacter* UCLSeatRegistry::FindHumanPawn() const
{
	UWorld* World = WorldFromOuter(this);
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ACLPlayerCharacter> It(World); It; ++It)
	{
		ACLPlayerCharacter* Char = *It;
		if (Char && Char->IsLocallyControlled() && !Char->IsA<ACLCombatPawn>())
		{
			return Char;
		}
	}
	return nullptr;
}

int32 UCLSeatRegistry::ReadyCount() const
{
	int32 Count = 0;
	for (const UCLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->IsReady())
		{
			++Count;
		}
	}
	return Count;
}

UCLParticipantSeat* UCLSeatRegistry::MakeSeat(const FString& DisplayName, UClass* MotorClass, const FGuid& ExistingId, const FCLLobbyGate* Gate)
{
	UCLParticipantSeat* Seat = NewObject<UCLParticipantSeat>(this);
	UCLSeatMotor* Book = NewObject<UCLSeatMotor>(Seat, MotorClass);
	Seat->Configure(ExistingId.IsValid() ? ExistingId : FGuid::NewGuid(), DisplayName, Book);
	if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Book))
	{
		if (Gate)
		{
			Remote->SetStaleSeconds(Gate->PlanStaleSeconds);
			Remote->SetLookaheadSeconds(Gate->PlanLookaheadSeconds);
		}
		else
		{
			float Stale = 3.f;
			float Lookahead = 0.75f;
			GConfig->GetFloat(TEXT("/Script/Calling.CLLobbySettings"), TEXT("PlanStaleSeconds"), Stale, GGameIni);
			GConfig->GetFloat(TEXT("/Script/Calling.CLLobbySettings"), TEXT("PlanLookaheadSeconds"), Lookahead, GGameIni);
			Remote->SetStaleSeconds(Stale);
			Remote->SetLookaheadSeconds(Lookahead);
		}
	}
	Seats.Add(Seat);
	return Seat;
}

UCLParticipantSeat* UCLSeatRegistry::EnsureLocalHuman(const FString& ProfileName, const FCLLobbyGate* Gate)
{
	if (UCLParticipantSeat* Host = FindHost())
	{
		return Host;
	}

	const FString Name = ProfileName.IsEmpty() ? TEXT("Host") : ProfileName;
	UCLParticipantSeat* Seat = MakeSeat(Name, UCLHumanSeatMotor::StaticClass(), FGuid(), Gate);
	Seat->SetHost(true);
	Seat->SetTeam(ECLPvpTeam::Red);
	if (ACLPlayerCharacter* Pawn = FindHumanPawn())
	{
		if (UCLPossessionComponent* Possession = Pawn->GetPossession())
		{
			Possession->PossessOwn(Pawn);
			Seat->SetPossession(Possession);
			Seat->SetAnchor(Pawn);
		}
	}
	return Seat;
}

UCLParticipantSeat* UCLSeatRegistry::EnsureNetHuman(APlayerController* PC, const FString& ProfileName, const FCLLobbyGate* Gate)
{
	if (!PC)
	{
		return nullptr;
	}
	if (UCLParticipantSeat* Existing = FindForController(PC))
	{
		Existing->BindController(PC);
		if (APawn* Pawn = PC->GetPawn())
		{
			Existing->SetAnchor(Pawn);
			if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Pawn))
			{
				if (UCLPossessionComponent* Possession = Char->GetPossession())
				{
					Possession->PossessOwn(Char);
					Existing->SetPossession(Possession);
				}
			}
		}
		return Existing;
	}

	if (PC->IsLocalController())
	{
		UCLParticipantSeat* Host = FindHost();
		if (Host && Host->GetBoundController() == nullptr)
		{
			Host->BindController(PC);
			return EnsureNetHuman(PC, ProfileName, Gate);
		}
		if (!Host)
		{
			UCLParticipantSeat* Seat = EnsureLocalHuman(ProfileName, Gate);
			if (Seat)
			{
				Seat->BindController(PC);
			}
			return Seat;
		}
	}

	const FString Name = ProfileName.IsEmpty() ? TEXT("Guest") : ProfileName;
	UCLParticipantSeat* Seat = MakeSeat(Name, UCLHumanSeatMotor::StaticClass(), FGuid(), Gate);
	Seat->SetHost(false);
	Seat->SetTeam(ECLPvpTeam::Blue);
	Seat->BindController(PC);
	if (APawn* Pawn = PC->GetPawn())
	{
		Seat->SetAnchor(Pawn);
		if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Pawn))
		{
			if (UCLPossessionComponent* Possession = Char->GetPossession())
			{
				Possession->PossessOwn(Char);
				Seat->SetPossession(Possession);
			}
		}
	}
	return Seat;
}

void UCLSeatRegistry::RemoveForController(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	for (int32 i = Seats.Num() - 1; i >= 0; --i)
	{
		UCLParticipantSeat* Seat = Seats[i];
		if (Seat && Seat->GetBoundController() == Controller && !Seat->IsHost())
		{
			Seats.RemoveAt(i);
		}
	}
}

UClass* UCLSeatRegistry::SeatMotorClassFromKind(const FString& Kind)
{
	if (Kind == TEXT("cursor"))
	{
		return UCLCursorSeatMotor::StaticClass();
	}
	if (Kind == TEXT("remoteAgent"))
	{
		return UCLRemoteAgentSeatMotor::StaticClass();
	}
	if (Kind == TEXT("algorithmic"))
	{
		return UCLAlgorithmicSeatMotor::StaticClass();
	}
	return UCLHumanSeatMotor::StaticClass();
}

APawn* UCLSeatRegistry::SpawnAgentPawn(ECLPvpTeam Team) const
{
	UWorld* World = WorldFromOuter(this);
	if (!World)
	{
		return nullptr;
	}
	FVector Loc = FVector(0.f, 250.f, 130.f);
	FRotator Rot = FRotator::ZeroRotator;
	if (AActor* Start = FindTeamPlayerStart(Team))
	{
		Loc = Start->GetActorLocation();
		Rot = Start->GetActorRotation();
		if (const APlayerStart* Ps = Cast<APlayerStart>(Start))
		{
			if (Ps->PlayerStartTag.IsNone())
			{
				Loc += FVector(0.f, 280.f, 0.f);
			}
		}
	}
	else
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Loc = It->GetActorLocation() + FVector(0.f, 280.f, 0.f);
			Rot = It->GetActorRotation();
			break;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FTransform Xform(Rot, Loc);
	ACLCombatPawn* Pawn = World->SpawnActorDeferred<ACLCombatPawn>(
		ACLCombatPawn::StaticClass(), Xform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Pawn)
	{
		return nullptr;
	}
	Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
	Pawn->AIControllerClass = nullptr;
	UGameplayStatics::FinishSpawningActor(Pawn, Xform);
	if (AController* Existing = Pawn->GetController())
	{
		Existing->UnPossess();
		Existing->Destroy();
	}
	ACLSeatController* Ctrl = World->SpawnActor<ACLSeatController>(ACLSeatController::StaticClass(), Loc, Rot, Params);
	if (Ctrl)
	{
		Ctrl->Possess(Pawn);
		Ctrl->SetControlRotation(Rot);
	}
	return Pawn;
}

AActor* UCLSeatRegistry::FindTeamPlayerStart(ECLPvpTeam Team) const
{
	UWorld* World = WorldFromOuter(this);
	if (!World)
	{
		return nullptr;
	}
	const FName Tag = Team == ECLPvpTeam::Blue ? FName(TEXT("Blue")) : FName(TEXT("Red"));
	AActor* Fallback = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->IsA<APlayerStartPIE>())
		{
			continue;
		}
		if (It->PlayerStartTag == Tag)
		{
			return *It;
		}
		if (!Fallback)
		{
			Fallback = *It;
		}
	}
	return Fallback;
}
