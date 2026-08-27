#include "Game/DLGameModeBase.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLGreyboxFloors.h"
#include "Core/DLLog.h"
#include "UI/DLComposerMenu.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ADLComposerGameMode::ADLComposerGameMode()
{
	SceneId = EDLSceneId::Composer;
	GameStateClass = ADLComposerGameState::StaticClass();
}

void ADLComposerGameMode::EnsureComposerGreybox()
{
	if (ADLGreyboxFloors* Floors = ADLGreyboxFloors::SpawnIfMissing(GetWorld(), EDLGreyboxLayout::SocialSquare))
	{
		ADLGreyboxFloors::EnsurePlayerStart(GetWorld(), Floors->GetPlayerStartLocation());
	}
	else
	{
		ADLGreyboxFloors::EnsurePlayerStart(GetWorld(), FVector(0.f, 0.f, 200.f));
	}
}

void ADLComposerGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	EnsureComposerGreybox();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsureComposerGreybox();
}

AActor* ADLComposerGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	EnsureComposerGreybox();
	if (ADLGreyboxFloors* Floors = ADLGreyboxFloors::SpawnIfMissing(GetWorld(), EDLGreyboxLayout::SocialSquare))
	{
		return ADLGreyboxFloors::EnsurePlayerStart(GetWorld(), Floors->GetPlayerStartLocation());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADLComposerGameMode::ShowComposerMenu()
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

	ComposerMenu = CreateWidget<UDLComposerMenu>(PC, UDLComposerMenu::StaticClass());
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

void ADLComposerGameMode::StartPlay()
{
	EnsureComposerGreybox();
	Super::StartPlay();
	EnsureComposerGreybox();
	ShowComposerMenu();

	if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
	{
		Lobby->BeginComposerScene();
	}
	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike: composer scene live"));

	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ADLComposerGameMode::HandleLobbyGo()
{
	UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>();
	if (Lobby)
	{
		Lobby->StampRosterOntoInvoice();
	}
	if (UDLSceneRouter* Router = GetGameInstance()->GetSubsystem<UDLSceneRouter>())
	{
		Router->TravelToScene(EDLSceneId::Pvp);
	}
}
