#include "Game/CLGameModeBase.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLGreyboxFloors.h"
#include "Core/CLLog.h"
#include "UI/CLComposerMenu.h"
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

	if (ComposerMenu)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	ComposerMenu = CreateWidget<UCLComposerMenu>(PC, UCLComposerMenu::StaticClass());
	if (ComposerMenu)
	{
		ComposerMenu->AddToViewport(25);
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetWidgetToFocus(ComposerMenu->TakeWidget());
		PC->SetInputMode(Mode);
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
