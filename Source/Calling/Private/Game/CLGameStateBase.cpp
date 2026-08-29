#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbyTypes.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ACLGameStateBase::ACLGameStateBase()
{
	ActivityState = CreateDefaultSubobject<UCLActivityStateComponent>(TEXT("ActivityState"));
}

void ACLGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACLGameStateBase, SceneId);
	DOREPLIFETIME(ACLGameStateBase, SocialPvpMode);
	DOREPLIFETIME(ACLGameStateBase, RaidChamberIndex);
	DOREPLIFETIME(ACLGameStateBase, TeamAScore);
	DOREPLIFETIME(ACLGameStateBase, TeamBScore);
	DOREPLIFETIME(ACLGameStateBase, SeatScores);
	DOREPLIFETIME(ACLGameStateBase, LobbySeats);
	DOREPLIFETIME(ACLGameStateBase, LobbyReady);
	DOREPLIFETIME(ACLGameStateBase, LobbyMinPlayers);
	DOREPLIFETIME(ACLGameStateBase, bLobbyStartQueued);
}

void ACLGameStateBase::SetSceneId(ECLSceneId InSceneId)
{
	if (HasAuthority())
	{
		SceneId = InSceneId;
	}
}

void ACLGameStateBase::SetSocialPvpMode(ECLSocialPvpMode Mode)
{
	if (HasAuthority())
	{
		SocialPvpMode = Mode;
	}
}

void ACLGameStateBase::SetRaidChamberIndex(int32 Index)
{
	if (HasAuthority())
	{
		RaidChamberIndex = Index;
	}
}

void ACLGameStateBase::SetLobbySnapshot(const TArray<FCLLobbySeatSnap>& Seats, int32 Ready, int32 MinPlayers, bool bQueued)
{
	if (!HasAuthority())
	{
		return;
	}
	LobbySeats = Seats;
	LobbyReady = Ready;
	LobbyMinPlayers = MinPlayers;
	bLobbyStartQueued = bQueued;
}

bool ACLGameStateBase::CanPlayersDamageEachOther() const
{
	if (SceneId == ECLSceneId::Pvp)
	{
		return true;
	}
	if (SceneId == ECLSceneId::Social)
	{
		return SocialPvpMode == ECLSocialPvpMode::Forced;
	}
	return false;
}

void ACLGameStateBase::RegisterTakeOut(AController* Killer, APawn* Victim, const TArray<AController*>& Assists)
{
	auto FindOrAddSeat = [this](AController* Ctrl) -> FCLSeatScore*
	{
		if (!Ctrl)
		{
			return nullptr;
		}
		FGuid SeatId;
		FString Name = Ctrl->GetName();
		ECLPvpTeam Team = ECLPvpTeam::Unassigned;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
			{
				if (const UCLParticipantSeat* Seat = Lobby->FindSeatForController(Ctrl))
				{
					SeatId = Seat->GetSeatId();
					Name = Seat->GetDisplayName();
					Team = Seat->GetTeam();
				}
			}
		}
		FCLSeatScore* Found = SeatScores.FindByPredicate([&](const FCLSeatScore& S)
		{
			return (SeatId.IsValid() && S.SeatId == SeatId) || (!SeatId.IsValid() && S.DisplayName == Name);
		});
		if (!Found)
		{
			FCLSeatScore NewScore;
			NewScore.SeatId = SeatId;
			NewScore.DisplayName = Name;
			SeatScores.Add(NewScore);
			Found = &SeatScores.Last();
		}
		(void)Team;
		return Found;
	};

	auto TeamOf = [](AController* Ctrl, UWorld* World) -> ECLPvpTeam
	{
		if (!Ctrl || !World)
		{
			return ECLPvpTeam::Unassigned;
		}
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
			{
				if (const UCLParticipantSeat* Seat = Lobby->FindSeatForController(Ctrl))
				{
					return Seat->GetTeam();
				}
			}
		}
		return ECLPvpTeam::Unassigned;
	};

	if (FCLSeatScore* KillScore = FindOrAddSeat(Killer))
	{
		KillScore->Score += 1.f;
		KillScore->FinalBlows += 1;
	}
	const ECLPvpTeam KillTeam = TeamOf(Killer, GetWorld());
	if (KillTeam == ECLPvpTeam::Red)
	{
		TeamAScore += 1.f;
	}
	else if (KillTeam == ECLPvpTeam::Blue)
	{
		TeamBScore += 1.f;
	}
	else if (Killer)
	{
		TeamAScore += 1.f;
	}

	for (AController* Assist : Assists)
	{
		if (!Assist || Assist == Killer)
		{
			continue;
		}
		if (FCLSeatScore* AssistScore = FindOrAddSeat(Assist))
		{
			AssistScore->Score += 0.25f;
			AssistScore->Assists += 1;
		}
		const ECLPvpTeam AssistTeam = TeamOf(Assist, GetWorld());
		if (AssistTeam == ECLPvpTeam::Red)
		{
			TeamAScore += 0.25f;
		}
		else if (AssistTeam == ECLPvpTeam::Blue)
		{
			TeamBScore += 0.25f;
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			if (Profiles->HasActiveProfile())
			{
				if (FCLLocalProfile* Prof = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId))
				{
					if (Killer && Killer == GetWorld()->GetFirstPlayerController())
					{
						Prof->Stats.Kills += 1;
					}
					if (Victim && Victim->GetController() == GetWorld()->GetFirstPlayerController())
					{
						Prof->Stats.Deaths += 1;
					}
					for (AController* Assist : Assists)
					{
						if (Assist == GetWorld()->GetFirstPlayerController())
						{
							Prof->Stats.Assists += 1;
						}
					}
					Profiles->SaveActiveProfile();
				}
			}
		}
	}
	(void)Victim;
}

FString ACLGameStateBase::GetScoreLine() const
{
	FString Seats;
	for (const FCLSeatScore& S : SeatScores)
	{
		if (!Seats.IsEmpty())
		{
			Seats += TEXT("  ");
		}
		Seats += FString::Printf(TEXT("%s %.2f"), S.DisplayName.IsEmpty() ? TEXT("?") : *S.DisplayName, S.Score);
	}
	if (Seats.IsEmpty())
	{
		return FString::Printf(TEXT("R %.2f   B %.2f"), TeamAScore, TeamBScore);
	}
	return FString::Printf(TEXT("R %.2f  B %.2f  |  %s"), TeamAScore, TeamBScore, *Seats);
}

ACLBootGameState::ACLBootGameState()
{
	SceneId = ECLSceneId::Boot;
}

ACLSocialGameState::ACLSocialGameState()
{
	SceneId = ECLSceneId::Social;
}

ACLComposerGameState::ACLComposerGameState()
{
	SceneId = ECLSceneId::Composer;
}

ACLPvpGameState::ACLPvpGameState()
{
	SceneId = ECLSceneId::Pvp;
}

void ACLPvpGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

ACLRaidGameState::ACLRaidGameState()
{
	SceneId = ECLSceneId::Raid;
}

void ACLRaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACLRaidGameState, ChambersCompleted);
	DOREPLIFETIME(ACLRaidGameState, bChamberCleared);
}

ACLPracticeGameState::ACLPracticeGameState()
{
	SceneId = ECLSceneId::Practice;
}
