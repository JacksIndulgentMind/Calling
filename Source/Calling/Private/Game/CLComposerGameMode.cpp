#include "Game/CLGameModeBase.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLGreyboxFloors.h"
#include "Core/CLLog.h"
#include "UI/CLComposerMenu.h"
#include "Game/CLLoopbackJoin.h"
#include "Game/CLParticipantSeat.h"
#include "Player/CLPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ACLComposerGameMode::ACLComposerGameMode()
{
	SceneId = ECLSceneId::Composer;
	GameStateClass = ACLComposerGameState::StaticClass();
}

void ACLComposerGameMode::EnsureComposerGreybox()
{
	if (ACLGreyboxFloors* Floors = ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::SocialSquare))
	{
		ACLGreyboxFloors::EnsurePlayerStart(GetWorld(), Floors->GetPlayerStartLocation());
	}
	else
	{
		ACLGreyboxFloors::EnsurePlayerStart(GetWorld(), FVector(0.f, 0.f, 200.f));
	}
}

void ACLComposerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	EnsureComposerGreybox();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsureComposerGreybox();
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		if (UCLParticipantSeat* Seat = Lobby->EnsureNetHumanSeat(NewPlayer))
		{
			if (NewPlayer && !NewPlayer->IsLocalController() && !Seat->IsReady())
			{
				Lobby->SetReady(Seat->GetSeatId(), true);
			}
		}
	}
	if (ACLPlayerController* PC = Cast<ACLPlayerController>(NewPlayer))
	{
		PC->EnsureComposerMenu();
	}
}

void ACLComposerGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->EnsureNetHumanSeat(NewPlayer);
	}
}

void ACLComposerGameMode::Logout(AController* Exiting)
{
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->RemoveSeatForController(Exiting);
	}
	Super::Logout(Exiting);
}

AActor* ACLComposerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	EnsureComposerGreybox();
	if (ACLGreyboxFloors* Floors = ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::SocialSquare))
	{
		return ACLGreyboxFloors::EnsurePlayerStart(GetWorld(), Floors->GetPlayerStartLocation());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ACLComposerGameMode::ShowComposerMenu()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(99202, 60.f, FColor::Cyan, TEXT("Compose PvP"));
	}

	if (ACLPlayerController* PC = Cast<ACLPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
	{
		PC->EnsureComposerMenu();
	}
}

void ACLComposerGameMode::StartPlay()
{
	EnsureComposerGreybox();
	Super::StartPlay();
	EnsureComposerGreybox();
	ShowComposerMenu();

	if (UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>())
	{
		Lobby->BeginComposerScene();
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Lobby->EnsureNetHumanSeat(PC);
		}
	}
	if (GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer)
	{
		CLLoopbackJoin::WriteBeacon(GetWorld());
	}
	UE_LOG(LogCalling, Display, TEXT("Calling: composer scene live"));

	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ACLComposerGameMode::HandleLobbyGo()
{
	UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>();
	if (Lobby)
	{
		Lobby->StampRosterOntoInvoice();
	}
	if (UCLSceneRouter* Router = GetGameInstance()->GetSubsystem<UCLSceneRouter>())
	{
		Router->TravelToScene(ECLSceneId::Pvp);
	}
}
