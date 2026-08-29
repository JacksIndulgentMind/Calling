#include "Game/CLGameModeBase.h"
#include "Game/CLAbilitySmoke.h"
#include "Core/CLLog.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSessionSubsystem.h"
#include "Loot/CLLootRulesService.h"
#include "AI/CLEncounterDirector.h"
#include "AI/CLPracticeDummy.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLHealthShieldComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Player/CLPlayerController.h"
#include "Player/CLVanguardCharacter.h"
#include "Player/CLPathfinderCharacter.h"
#include "Player/CLWardenCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Ability/CLAbilityCatalog.h"
#include "Ability/CLAbility.h"
#include "HAL/IConsoleManager.h"
#include "UI/CLBootProfileWidget.h"
#include "UI/CLSocialMarkerWidget.h"
#include "Game/CLGreyboxFloors.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/PlayerStartPIE.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
ACLPvpGameMode::ACLPvpGameMode()
{
	SceneId = ECLSceneId::Pvp;
	GameStateClass = ACLPvpGameState::StaticClass();
}

void ACLPvpGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::PvpThreeLane);
}

void ACLPvpGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->EnsureNetHumanSeat(NewPlayer);
	}
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* ACLPvpGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::PvpThreeLane);
	FName Tag = FName(TEXT("Red"));
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		if (const UCLParticipantSeat* Seat = Lobby->FindSeatForController(Player))
		{
			if (Seat->GetTeam() == ECLPvpTeam::Blue)
			{
				Tag = FName(TEXT("Blue"));
			}
		}
	}
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (!It->IsA<APlayerStartPIE>() && It->PlayerStartTag == Tag)
		{
			return *It;
		}
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ACLPvpGameMode::StartPlay()
{
	Super::StartPlay();
	for (TActorIterator<ACLGreyboxFloors> It(GetWorld()); It; ++It)
	{
		It->RebuildNavigation();
	}
	UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>();
	if (Lobby)
	{
		Lobby->BeginPvpOrRestore();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				Lobby->EnsureNetHumanSeat(PC);
			}
		}
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		if (Lobby && Lobby->IsGameplayUnlocked() && !Lobby->HasGate())
		{
			Activity->BeginInProgress();
		}
		else
		{
			Activity->BeginLobby();
		}
	}
}

void ACLPvpGameMode::HandleLobbyGo()
{
	StartMatchFromLobby();
}

void ACLPvpGameMode::StartMatchFromLobby()
{
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ACLPvpGameMode::EndMatchAndAward()
{
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AwardDropFromTable(FName(TEXT("pvp_match_complete")), It->Get());
	}

	if (ACLPvpGameState* GS = GetGameState<ACLPvpGameState>())
	{
		(void)GS->GetTeamAScore();
	}
}
