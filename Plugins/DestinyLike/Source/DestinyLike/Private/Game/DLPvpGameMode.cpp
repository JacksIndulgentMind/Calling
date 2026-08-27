#include "Game/DLGameModeBase.h"
#include "Game/DLAbilitySmoke.h"
#include "Core/DLLog.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLVaultSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLSessionSubsystem.h"
#include "Loot/DLLootRulesService.h"
#include "AI/DLEncounterDirector.h"
#include "AI/DLPracticeDummy.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLHealthShieldComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Player/DLPlayerController.h"
#include "Player/DLVanguardCharacter.h"
#include "Player/DLPathfinderCharacter.h"
#include "Player/DLWardenCharacter.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLAbilityLoadoutComponent.h"
#include "Ability/DLAbilityCatalog.h"
#include "Ability/DLAbility.h"
#include "HAL/IConsoleManager.h"
#include "UI/DLBootProfileWidget.h"
#include "UI/DLSocialMarkerWidget.h"
#include "Game/DLGreyboxFloors.h"
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
ADLPvpGameMode::ADLPvpGameMode()
{
	SceneId = EDLSceneId::Pvp;
	GameStateClass = ADLPvpGameState::StaticClass();
}

void ADLPvpGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ADLGreyboxFloors::SpawnIfMissing(GetWorld(), EDLGreyboxLayout::PvpThreeLane);
}

AActor* ADLPvpGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	ADLGreyboxFloors::SpawnIfMissing(GetWorld(), EDLGreyboxLayout::PvpThreeLane);
	FName Tag = FName(TEXT("Red"));
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		if (const UDLParticipantSeat* Seat = Lobby->FindSeatForController(Player))
		{
			if (Seat->GetTeam() == EDLPvpTeam::Blue)
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

void ADLPvpGameMode::StartPlay()
{
	Super::StartPlay();
	UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>();
	if (Lobby)
	{
		Lobby->BeginPvpOrRestore();
	}
	if (UDLActivityStateComponent* Activity = GetActivityState())
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

void ADLPvpGameMode::HandleLobbyGo()
{
	StartMatchFromLobby();
}

void ADLPvpGameMode::StartMatchFromLobby()
{
	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ADLPvpGameMode::EndMatchAndAward()
{
	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AwardDropFromTable(FName(TEXT("pvp_match_complete")), It->Get());
	}

	if (ADLPvpGameState* GS = GetGameState<ADLPvpGameState>())
	{
		(void)GS->GetTeamAScore();
	}
}
