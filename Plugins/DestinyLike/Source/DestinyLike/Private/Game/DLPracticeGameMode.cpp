#include "Game/DLGameModeBase.h"
#include "Game/DLAbilitySmoke.h"
#include "Core/DLLog.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLVaultSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLLobbySubsystem.h"
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
ADLPracticeGameMode::ADLPracticeGameMode()
{
	SceneId = EDLSceneId::Practice;
	GameStateClass = ADLPracticeGameState::StaticClass();
}

void ADLPracticeGameMode::StartPlay()
{
	Super::StartPlay();
	if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
	{
		Lobby->ClearScene();
	}
	SpawnPracticeDummies(4);
}

void ADLPracticeGameMode::SpawnPracticeDummies(int32 Count)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = FVector(0.f, 0.f, 100.f);
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Loc = Origin + FVector(300.f * (i + 1), 0.f, 0.f);
		World->SpawnActor<ADLPracticeDummy>(ADLPracticeDummy::StaticClass(), Loc, FRotator::ZeroRotator);
	}
}
