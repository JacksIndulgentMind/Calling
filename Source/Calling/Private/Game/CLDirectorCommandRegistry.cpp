#include "Game/CLDirectorCommandRegistry.h"
#include "Game/CLActivityLauncher.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLSessionSubsystem.h"
#include "Player/CLPlayerController.h"
#include "UI/CLMainMenuOverlay.h"
#include "Game/CLLobbyTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

TSharedRef<FJsonObject> FCLDirectorCommandRegistry::Dispatch(
	UGameInstance* GI,
	ACLPlayerController* PC,
	const FString& InAction,
	FGuid* AgentSeatId,
	const TSharedPtr<FJsonObject>& Args)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	FString Action = InAction.ToLower();
	if (Action.IsEmpty())
	{
		Action = TEXT("toggle");
	}

	if (!GI)
	{
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), TEXT("no_game"));
		return Out;
	}

	if (!PC)
	{
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), TEXT("no_local_controller"));
		return Out;
	}

	UCLMainMenuOverlay* Menu = PC->GetMainMenu();
	const bool bMenuOptional = Action == TEXT("savedefaultsocial");
	if (!Menu && !bMenuOptional)
	{
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), TEXT("no_menu"));
		return Out;
	}

	UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>();
	UWorld* World = GI->GetWorld();
	if (World && World->GetNetMode() == NM_Client)
	{
		const bool bHostOnly =
			Action == TEXT("virtualhost") || Action == TEXT("ready") || Action == TEXT("go")
			|| Action == TEXT("start") || Action == TEXT("host") || Action == TEXT("guest")
			|| Action == TEXT("pvp") || Action == TEXT("composer")
			|| Action == TEXT("join");
		if (bHostOnly)
		{
			Out->SetBoolField(TEXT("ok"), false);
			Out->SetStringField(TEXT("error"), TEXT("host_only"));
			return Out;
		}
	}

	if (Action == TEXT("open"))
	{
		PC->SetMainMenuOpen(true);
	}
	else if (Action == TEXT("close"))
	{
		PC->SetMainMenuOpen(false);
	}
	else if (Action == TEXT("toggle"))
	{
		PC->ToggleMainMenu();
	}
	else if (Action == TEXT("director") || Action == TEXT("directortab"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
	}
	else if (Action == TEXT("keybinds") || Action == TEXT("keybindstab"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowKeybindsTab();
	}
	else if (Action == TEXT("pvp") || Action == TEXT("composer"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		CLActivityLauncher::Travel(PC, ECLSceneId::Composer);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("arena"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		CLActivityLauncher::Travel(PC, ECLSceneId::Pvp);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("raid"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		CLActivityLauncher::Travel(PC, ECLSceneId::Raid);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("practice"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		CLActivityLauncher::Travel(PC, ECLSceneId::Practice);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("social"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		CLActivityLauncher::ExitToSocial(PC);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("ready"))
	{
		if (Lobby)
		{
			Lobby->ToggleLocalReady();
		}
	}
	else if (Action == TEXT("host"))
	{
		if (Lobby)
		{
			Lobby->ClaimLocalHost();
		}
	}
	else if (Action == TEXT("guest"))
	{
		if (Lobby)
		{
			Lobby->ClaimLocalGuest();
		}
	}
	else if (Action == TEXT("go") || Action == TEXT("start"))
	{
		if (Lobby)
		{
			Lobby->RequestLocalGo();
		}
	}
	else if (Action == TEXT("virtualhost"))
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->StartComposerLoopbackHost();
		}
		Menu->HideOverlay();
	}
	else if (Action == TEXT("virtualjoin"))
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->JoinLoopback(TEXT(""));
		}
		Menu->HideOverlay();
	}
	else if (Action == TEXT("lobbytab"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowLobbyTab();
	}
	else if (Action == TEXT("socialaudience"))
	{
		FString KindStr;
		if (Args.IsValid())
		{
			if (Args->HasField(TEXT("kind")))
			{
				KindStr = Args->GetStringField(TEXT("kind"));
			}
			else if (Args->HasField(TEXT("audience")))
			{
				KindStr = Args->GetStringField(TEXT("audience"));
			}
		}
		const ECLSocialDefaultKind Kind = FCLSocialDefault::KindFromString(KindStr);
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			if (Kind == ECLSocialDefaultKind::Join)
			{
				FString Host = TEXT("127.0.0.1");
				int32 Port = 7777;
				if (Args.IsValid())
				{
					if (Args->HasField(TEXT("host")))
					{
						Host = Args->GetStringField(TEXT("host"));
					}
					if (Args->HasField(TEXT("port")))
					{
						Port = static_cast<int32>(Args->GetNumberField(TEXT("port")));
					}
				}
				Sessions->JoinSocialHost(Host, Port);
			}
			else
			{
				Sessions->HostSocialAudience(Kind);
			}
		}
		Menu->HideOverlay();
	}
	else if (Action == TEXT("socialjoin"))
	{
		FString Host = TEXT("127.0.0.1");
		int32 Port = 7777;
		if (Args.IsValid())
		{
			if (Args->HasField(TEXT("host")))
			{
				Host = Args->GetStringField(TEXT("host"));
			}
			if (Args->HasField(TEXT("port")))
			{
				Port = static_cast<int32>(Args->GetNumberField(TEXT("port")));
			}
		}
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->JoinSocialHost(Host, Port);
		}
		Menu->HideOverlay();
	}
	else if (Action == TEXT("savedefaultsocial"))
	{
		ECLSocialDefaultKind Kind = ECLSocialDefaultKind::Private;
		FString Host = TEXT("127.0.0.1");
		int32 Port = 7777;
		ECLSocialJoinFallback Fallback = ECLSocialJoinFallback::Private;
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Kind = Sessions->GetLiveSocialKind();
		}
		if (Args.IsValid())
		{
			FString KindRaw;
			if (Args->TryGetStringField(TEXT("kind"), KindRaw) || Args->TryGetStringField(TEXT("Kind"), KindRaw))
			{
				Kind = FCLSocialDefault::KindFromString(KindRaw);
			}
			if (Args->HasField(TEXT("host")))
			{
				Host = Args->GetStringField(TEXT("host"));
			}
			if (Args->HasField(TEXT("port")))
			{
				Port = static_cast<int32>(Args->GetNumberField(TEXT("port")));
			}
			if (Args->HasField(TEXT("fallback")))
			{
				Fallback = FCLSocialDefault::FallbackFromString(Args->GetStringField(TEXT("fallback")));
			}
		}
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			if (!Sessions->SaveSocialDefault(Kind, Host, Port, Fallback))
			{
				Out->SetBoolField(TEXT("ok"), false);
				Out->SetStringField(TEXT("error"), TEXT("save_social_default_failed"));
				Out->SetStringField(TEXT("action"), Action);
				return Out;
			}
			if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
			{
				const FCLSocialDefault Saved = Profiles->GetSocialDefault();
				Out->SetStringField(TEXT("savedKind"), FCLSocialDefault::KindToString(Saved.Kind));
				Out->SetStringField(TEXT("savedHost"), Saved.JoinHost);
				Out->SetNumberField(TEXT("savedPort"), Saved.JoinPort);
				Out->SetStringField(TEXT("savedFallback"), FCLSocialDefault::FallbackToString(Saved.JoinFallback));
			}
		}
	}
	else if (Action == TEXT("join"))
	{
		if (Lobby)
		{
			FString Error;
			if (UCLParticipantSeat* Seat = Lobby->JoinRemoteAgent(TEXT("cursor"), false, Error, TEXT("cursor")))
			{
				if (AgentSeatId)
				{
					*AgentSeatId = Seat->GetSeatId();
				}
			}
		}
	}
	else
	{
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), TEXT("unknown_action"));
		return Out;
	}

	Out->SetBoolField(TEXT("ok"), true);
	Out->SetStringField(TEXT("action"), Action);
	return Out;
}
