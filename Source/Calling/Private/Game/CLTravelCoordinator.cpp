#include "Game/CLTravelCoordinator.h"
#include "Game/CLInvoiceService.h"
#include "Game/CLSeatRegistry.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLControllerPlaybook.h"
#include "Game/CLLobbyTypes.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLHeadlessAgent.h"
#include "Player/CLPossessionComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

namespace
{
	UWorld* WorldFromOuter(const UObject* Obj)
	{
		const UObject* Outer = Obj ? Obj->GetOuter() : nullptr;
		return Outer ? Outer->GetWorld() : nullptr;
	}

	void RecreateSeatsFromRoster(UCLInvoiceService* Invoices, UCLSeatRegistry* Seats, const FCLLobbyGate* Gate)
	{
		if (!Invoices || !Seats)
		{
			return;
		}
		const FCLLobbyInvoice* Live = Invoices->GetLive();
		if (!Live || Live->Roster.Num() == 0)
		{
			return;
		}
		Seats->Reset();
		for (const FCLInvoiceSeat& Row : Live->Roster)
		{
			UCLParticipantSeat* Seat = Seats->MakeSeat(Row.DisplayName, UCLSeatRegistry::PlaybookClassFromKind(Row.Kind), Row.SeatId, Gate);
			Seat->SetTeam(Row.Team);
			Seat->SetHeadlessJoin(Row.bHeadless);
			Seat->SetDriveSeatId(Row.DriveSeatId.IsValid() ? Row.DriveSeatId : Row.SeatId);
			if (Row.Kind == TEXT("human"))
			{
				Seat->SetHost(true);
			}
		}
	}
}

void UCLTravelCoordinator::StampRosterOntoInvoice(UCLInvoiceService* Invoices, UCLSeatRegistry* Seats)
{
	if (!Invoices || !Seats)
	{
		return;
	}
	UCLInvoiceBox* Live = Invoices->GetLiveBox();
	if (!Live)
	{
		return;
	}
	Live->Value.Roster.Reset();
	Live->Value.Activity = ECLSceneId::Pvp;
	for (const UCLParticipantSeat* Seat : Seats->GetAll())
	{
		if (!Seat)
		{
			continue;
		}
		FCLInvoiceSeat Row;
		Row.SeatId = Seat->GetSeatId();
		Row.DisplayName = Seat->GetDisplayName();
		Row.Team = Seat->GetTeam();
		Row.Kind = Seat->GetPlaybook() ? Seat->GetPlaybook()->GetKindId() : TEXT("none");
		Row.DriveSeatId = Seat->GetDriveSeatId();
		Row.bHeadless = Seat->IsHeadlessJoin();
		Live->Value.Roster.Add(Row);
	}
	Invoices->SetPending(Live->Value);
}

void UCLTravelCoordinator::RestoreBodiesAfterTravel(UCLInvoiceService* Invoices, UCLSeatRegistry* Seats, const FCLLobbyGate* Gate)
{
	if (!Seats)
	{
		return;
	}
	if (Seats->Num() == 0)
	{
		RecreateSeatsFromRoster(Invoices, Seats, Gate);
	}

	for (UCLParticipantSeat* Seat : Seats->GetAll())
	{
		if (!Seat)
		{
			continue;
		}
		if (UCLRemoteAgentPlaybook* Remote = Cast<UCLRemoteAgentPlaybook>(Seat->GetPlaybook()))
		{
			Remote->CancelPlan();
			Remote->CancelGoto();
		}
		Seat->SetPossession(nullptr);
		Seat->SetAnchor(nullptr);
	}

	if (UCLParticipantSeat* Host = Seats->FindHost())
	{
		if (ACLPlayerCharacter* Human = Seats->FindHumanPawn())
		{
			if (UCLPossessionComponent* Possession = Human->GetPossession())
			{
				Possession->PossessOwn(Human);
				Host->SetPossession(Possession);
				Host->SetAnchor(Human);
			}
			if (AActor* Start = Seats->FindTeamPlayerStart(Host->GetTeam()))
			{
				Human->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation(), false, true);
			}
		}
	}

	UWorld* World = WorldFromOuter(Seats);
	if (!World)
	{
		return;
	}

	for (UCLParticipantSeat* Seat : Seats->GetAll())
	{
		if (!Seat || Seat->IsHost())
		{
			continue;
		}
		if (!Seat->GetPlaybook() || !Seat->GetPlaybook()->IsA<UCLRemoteAgentPlaybook>())
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (Seat->IsHeadlessJoin())
		{
			ACLHeadlessAgent* Anchor = World->SpawnActor<ACLHeadlessAgent>(ACLHeadlessAgent::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (!Anchor)
			{
				continue;
			}
			Seat->SetAnchor(Anchor);
			Seat->SetPossession(Anchor->GetPossession());
			Anchor->GetPossession()->GoHeadless();
		}

		const FGuid DriveId = Seat->GetDriveSeatId();
		const bool bOwnBody = !DriveId.IsValid() || DriveId == Seat->GetSeatId();
		if (bOwnBody)
		{
			if (APawn* Body = Seats->SpawnAgentPawn(Seat->GetTeam()))
			{
				if (Seat->GetPossession())
				{
					Seat->GetPossession()->MindControl(Body);
				}
				else if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Body))
				{
					Char->GetPossession()->PossessOwn(Char);
					Seat->SetPossession(Char->GetPossession());
					Seat->SetAnchor(Char);
				}
				Seat->SetDriveSeatId(Seat->GetSeatId());
			}
		}
	}

	for (UCLParticipantSeat* Seat : Seats->GetAll())
	{
		if (!Seat || Seat->IsHost() || !Seat->GetPossession())
		{
			continue;
		}
		const FGuid DriveId = Seat->GetDriveSeatId();
		if (!DriveId.IsValid() || DriveId == Seat->GetSeatId())
		{
			continue;
		}
		if (UCLParticipantSeat* Target = Seats->Find(DriveId))
		{
			if (APawn* TargetPawn = Target->GetDrivenPawn())
			{
				Seat->GetPossession()->MindControl(TargetPawn);
			}
		}
	}
}
