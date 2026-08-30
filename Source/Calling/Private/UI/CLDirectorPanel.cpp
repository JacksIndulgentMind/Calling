#include "UI/CLDirectorPanel.h"
#include "UI/CLMainMenuOverlay.h"
#include "Blueprint/UserWidget.h"
#include "Game/CLActivityLauncher.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLSessionSubsystem.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	UGameInstance* DirectorGameInstance(const UCLDirectorPanel* Panel)
	{
		const UUserWidget* Host = Cast<UUserWidget>(Panel ? Panel->GetOuter() : nullptr);
		return Host ? Host->GetGameInstance() : nullptr;
	}

	void HideHostOverlay(UCLDirectorPanel* Panel)
	{
		if (UCLMainMenuOverlay* Overlay = Cast<UCLMainMenuOverlay>(Panel ? Panel->GetOuter() : nullptr))
		{
			Overlay->HideOverlay();
		}
	}
}

void UCLDirectorPanel::JumpToActivity(ECLSceneId Scene, int32 RaidChamberIndex)
{
	CLActivityLauncher::Travel(GetOuter(), Scene, RaidChamberIndex);
	HideHostOverlay(this);
}

void UCLDirectorPanel::ExitToSocial()
{
	CLActivityLauncher::ExitToSocial(GetOuter());
	HideHostOverlay(this);
}

void UCLDirectorPanel::UnsetDefaultProfile()
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			const FCLLocalProfile Active = Profiles->GetActiveProfile();
			if (Active.ProfileId.IsValid())
			{
				Profiles->SetDefaultProfile(Active.ProfileId, false);
			}
		}
	}
}

void UCLDirectorPanel::HostSocialLobby(ECLSocialPvpMode Mode)
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>();
		UCLSceneRouter* Router = GI->GetSubsystem<UCLSceneRouter>();
		if (Sessions && Router)
		{
			int32 MaxPlayers = 16;
			GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Social"), MaxPlayers, GGameIni);
			Sessions->HostSession(FCLLobbyInvoice::MakeSocial(ECLLobbyAccess::Open, Mode, MaxPlayers),
				Router->GetMapNameForScene(ECLSceneId::Social));
		}
	}
}

void UCLDirectorPanel::RefreshActivityLobbies(ECLSceneId Activity)
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->FindSessions(Activity);
		}
	}
}

void UCLDirectorPanel::JoinListedLobby(int32 Index)
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->JoinSessionByIndex(Index);
		}
	}
}

void UCLDirectorPanel::StartLoopbackHost()
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->StartComposerLoopbackHost();
		}
	}
	HideHostOverlay(this);
}

void UCLDirectorPanel::JoinLoopback(const FString& Selected)
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->JoinLoopback(Selected);
		}
	}
	HideHostOverlay(this);
}

void UCLDirectorPanel::ToggleLocalReady()
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->ToggleLocalReady();
		}
	}
}

void UCLDirectorPanel::JoinTeam(ECLPvpTeam Team)
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			if (UCLParticipantSeat* Local = Lobby->FindLocalSeat())
			{
				FString Error;
				Lobby->SetTeam(Local->GetSeatId(), Team, Error);
			}
		}
	}
}

void UCLDirectorPanel::RequestGo()
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->RequestLocalGo();
		}
	}
	HideHostOverlay(this);
}

void UCLDirectorPanel::HostSocialClosed()
{
	if (UGameInstance* GI = DirectorGameInstance(this))
	{
		UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>();
		UCLSceneRouter* Router = GI->GetSubsystem<UCLSceneRouter>();
		if (Sessions && Router)
		{
			int32 MaxPlayers = 16;
			GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Social"), MaxPlayers, GGameIni);
			Sessions->HostSession(FCLLobbyInvoice::MakeSocial(ECLLobbyAccess::Closed, ECLSocialPvpMode::Optional, MaxPlayers),
				Router->GetMapNameForScene(ECLSceneId::Social));
		}
	}
}
