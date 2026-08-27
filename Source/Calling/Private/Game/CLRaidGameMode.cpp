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
ACLRaidGameMode::ACLRaidGameMode()
{
	SceneId = ECLSceneId::Raid;
	GameStateClass = ACLRaidGameState::StaticClass();
	EncounterDirector = CreateDefaultSubobject<UCLEncounterDirector>(TEXT("EncounterDirector"));
}

void ACLRaidGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const int32 Chamber = InferChamberIndexFromMap();
	if (ACLGreyboxFloors* Floors = ACLGreyboxFloors::SpawnIfMissing(GetWorld(), GreyboxLayoutForChamber(Chamber)))
	{
		if (EncounterDirector)
		{
			EncounterDirector->ArenaHalfExtent = Floors->GetSuggestedArenaHalfExtent();
		}
	}
}

void ACLRaidGameMode::StartPlay()
{
	Super::StartPlay();
	if (UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>())
	{
		Lobby->BeginGatedScene(ECLSceneId::Raid);
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginLobby();
	}
}

void ACLRaidGameMode::HandleLobbyGo()
{
	BeginChamber(InferChamberIndexFromMap());
}

int32 ACLRaidGameMode::InferChamberIndexFromMap() const
{
	const FString MapName = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (MapName.Contains(TEXT("Raid_02")))
	{
		return 1;
	}
	if (MapName.Contains(TEXT("Raid_03")))
	{
		return 2;
	}
	if (MapName.Contains(TEXT("Raid_04")))
	{
		return 3;
	}
	return 0;
}

ECLGreyboxLayout ACLRaidGameMode::GreyboxLayoutForChamber(int32 ChamberIndex)
{
	switch (ChamberIndex)
	{
	case 1:
		return ECLGreyboxLayout::RaidApproach;
	case 2:
		return ECLGreyboxLayout::RaidArena;
	case 3:
		return ECLGreyboxLayout::RaidPit;
	default:
		return ECLGreyboxLayout::RaidCourt;
	}
}

void ACLRaidGameMode::BeginChamber(int32 ChamberIndex)
{
	ChamberIndex = FMath::Clamp(ChamberIndex, 0, ChamberCount - 1);
	if (ACLRaidGameState* GS = GetGameState<ACLRaidGameState>())
	{
		GS->SetRaidChamberIndex(ChamberIndex);
		GS->bChamberCleared = false;
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
	if (EncounterDirector)
	{
		EncounterDirector->BuildAndSpawnChamber(ChamberIndex);
	}
}

void ACLRaidGameMode::CompleteChamber()
{
	ACLRaidGameState* GS = GetGameState<ACLRaidGameState>();
	if (!GS)
	{
		return;
	}

	GS->bChamberCleared = true;
	GS->ChambersCompleted = GS->GetRaidChamberIndex() + 1;

	const FName TableId(*FString::Printf(TEXT("raid_chamber_%d"), GS->GetRaidChamberIndex() + 1));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AwardDropFromTable(TableId, It->Get());
	}

	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
	}
}

void ACLRaidGameMode::AdvanceOrFinishRaid()
{
	ACLRaidGameState* GS = GetGameState<ACLRaidGameState>();
	if (!GS)
	{
		return;
	}

	const int32 Next = GS->GetRaidChamberIndex() + 1;
	if (Next >= ChamberCount)
	{
		if (UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
		{
			if (FCLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId))
			{
				Profile->Stats.RaidsCompleted += 1;
				Profiles->SaveActiveProfile();
			}
		}
		RequestExitToSocial();
		return;
	}

	if (UCLSceneRouter* Router = GetGameInstance()->GetSubsystem<UCLSceneRouter>())
	{
		Router->TravelToScene(ECLSceneId::Raid, Next);
	}
}
