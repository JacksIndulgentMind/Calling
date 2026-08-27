#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLLobbyTypes.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ADLGameStateBase::ADLGameStateBase()
{
	ActivityState = CreateDefaultSubobject<UDLActivityStateComponent>(TEXT("ActivityState"));
}

void ADLGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADLGameStateBase, SceneId);
	DOREPLIFETIME(ADLGameStateBase, SocialPvpMode);
	DOREPLIFETIME(ADLGameStateBase, RaidChamberIndex);
	DOREPLIFETIME(ADLGameStateBase, TeamAScore);
	DOREPLIFETIME(ADLGameStateBase, TeamBScore);
	DOREPLIFETIME(ADLGameStateBase, SeatScores);
}

void ADLGameStateBase::SetSceneId(EDLSceneId InSceneId)
{
	if (HasAuthority())
	{
		SceneId = InSceneId;
	}
}

void ADLGameStateBase::SetSocialPvpMode(EDLSocialPvpMode Mode)
{
	if (HasAuthority())
	{
		SocialPvpMode = Mode;
	}
}

void ADLGameStateBase::SetRaidChamberIndex(int32 Index)
{
	if (HasAuthority())
	{
		RaidChamberIndex = Index;
	}
}

bool ADLGameStateBase::CanPlayersDamageEachOther() const
{
	if (SceneId == EDLSceneId::Pvp)
	{
		return true;
	}
	if (SceneId == EDLSceneId::Social)
	{
		return SocialPvpMode == EDLSocialPvpMode::Forced;
	}
	return false;
}

void ADLGameStateBase::RegisterTakeOut(AController* Killer, APawn* Victim, const TArray<AController*>& Assists)
{
	auto FindOrAddSeat = [this](AController* Ctrl) -> FDLSeatScore*
	{
		if (!Ctrl)
		{
			return nullptr;
		}
		FGuid SeatId;
		FString Name = Ctrl->GetName();
		EDLPvpTeam Team = EDLPvpTeam::Unassigned;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
			{
				if (const UDLParticipantSeat* Seat = Lobby->FindSeatForController(Ctrl))
				{
					SeatId = Seat->GetSeatId();
					Name = Seat->GetDisplayName();
					Team = Seat->GetTeam();
				}
			}
		}
		FDLSeatScore* Found = SeatScores.FindByPredicate([&](const FDLSeatScore& S)
		{
			return (SeatId.IsValid() && S.SeatId == SeatId) || (!SeatId.IsValid() && S.DisplayName == Name);
		});
		if (!Found)
		{
			FDLSeatScore NewScore;
			NewScore.SeatId = SeatId;
			NewScore.DisplayName = Name;
			SeatScores.Add(NewScore);
			Found = &SeatScores.Last();
		}
		(void)Team;
		return Found;
	};

	auto TeamOf = [](AController* Ctrl, UWorld* World) -> EDLPvpTeam
	{
		if (!Ctrl || !World)
		{
			return EDLPvpTeam::Unassigned;
		}
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
			{
				if (const UDLParticipantSeat* Seat = Lobby->FindSeatForController(Ctrl))
				{
					return Seat->GetTeam();
				}
			}
		}
		return EDLPvpTeam::Unassigned;
	};

	if (FDLSeatScore* KillScore = FindOrAddSeat(Killer))
	{
		KillScore->Score += 1.f;
		KillScore->FinalBlows += 1;
	}
	const EDLPvpTeam KillTeam = TeamOf(Killer, GetWorld());
	if (KillTeam == EDLPvpTeam::Red)
	{
		TeamAScore += 1.f;
	}
	else if (KillTeam == EDLPvpTeam::Blue)
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
		if (FDLSeatScore* AssistScore = FindOrAddSeat(Assist))
		{
			AssistScore->Score += 0.25f;
			AssistScore->Assists += 1;
		}
		const EDLPvpTeam AssistTeam = TeamOf(Assist, GetWorld());
		if (AssistTeam == EDLPvpTeam::Red)
		{
			TeamAScore += 0.25f;
		}
		else if (AssistTeam == EDLPvpTeam::Blue)
		{
			TeamBScore += 0.25f;
		}
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLProfileSubsystem* Profiles = GI->GetSubsystem<UDLProfileSubsystem>())
		{
			if (Profiles->HasActiveProfile())
			{
				if (FDLLocalProfile* Prof = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId))
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

FString ADLGameStateBase::GetScoreLine() const
{
	FString Seats;
	for (const FDLSeatScore& S : SeatScores)
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

ADLBootGameState::ADLBootGameState()
{
	SceneId = EDLSceneId::Boot;
}

ADLSocialGameState::ADLSocialGameState()
{
	SceneId = EDLSceneId::Social;
}

ADLComposerGameState::ADLComposerGameState()
{
	SceneId = EDLSceneId::Composer;
}

ADLPvpGameState::ADLPvpGameState()
{
	SceneId = EDLSceneId::Pvp;
}

void ADLPvpGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

ADLRaidGameState::ADLRaidGameState()
{
	SceneId = EDLSceneId::Raid;
}

void ADLRaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADLRaidGameState, ChambersCompleted);
	DOREPLIFETIME(ADLRaidGameState, bChamberCleared);
}

ADLPracticeGameState::ADLPracticeGameState()
{
	SceneId = EDLSceneId::Practice;
}
