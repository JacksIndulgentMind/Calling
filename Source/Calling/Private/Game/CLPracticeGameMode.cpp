#include "Game/CLGameModeBase.h"
#include "Game/CLAbilitySmoke.h"
#include "Core/CLLog.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbySubsystem.h"
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
ACLPracticeGameMode::ACLPracticeGameMode()
{
	SceneId = ECLSceneId::Practice;
	GameStateClass = ACLPracticeGameState::StaticClass();
}

void ACLPracticeGameMode::StartPlay()
{
	Super::StartPlay();
	ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::PracticePillar);
	for (TActorIterator<ACLGreyboxFloors> It(GetWorld()); It; ++It)
	{
		It->RebuildNavigation();
	}
	if (UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>())
	{
		Lobby->ClearScene();
	}
	SpawnPracticeDummies(4);
}

void ACLPracticeGameMode::SpawnPracticeDummies(int32 Count)
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
		World->SpawnActor<ACLPracticeDummy>(ACLPracticeDummy::StaticClass(), Loc, FRotator::ZeroRotator);
	}
}
