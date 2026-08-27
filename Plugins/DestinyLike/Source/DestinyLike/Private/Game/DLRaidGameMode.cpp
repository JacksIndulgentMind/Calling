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
ADLRaidGameMode::ADLRaidGameMode()
{
	SceneId = EDLSceneId::Raid;
	GameStateClass = ADLRaidGameState::StaticClass();
	EncounterDirector = CreateDefaultSubobject<UDLEncounterDirector>(TEXT("EncounterDirector"));
}

void ADLRaidGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const int32 Chamber = InferChamberIndexFromMap();
	if (ADLGreyboxFloors* Floors = ADLGreyboxFloors::SpawnIfMissing(GetWorld(), GreyboxLayoutForChamber(Chamber)))
	{
		if (EncounterDirector)
		{
			EncounterDirector->ArenaHalfExtent = Floors->GetSuggestedArenaHalfExtent();
		}
	}
}

void ADLRaidGameMode::StartPlay()
{
	Super::StartPlay();
	if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
	{
		Lobby->BeginGatedScene(EDLSceneId::Raid);
	}
	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginLobby();
	}
}

void ADLRaidGameMode::HandleLobbyGo()
{
	BeginChamber(InferChamberIndexFromMap());
}

int32 ADLRaidGameMode::InferChamberIndexFromMap() const
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

EDLGreyboxLayout ADLRaidGameMode::GreyboxLayoutForChamber(int32 ChamberIndex)
{
	switch (ChamberIndex)
	{
	case 1:
		return EDLGreyboxLayout::RaidShuro;
	case 2:
		return EDLGreyboxLayout::RaidMorgeth;
	case 3:
		return EDLGreyboxLayout::RaidVault;
	default:
		return EDLGreyboxLayout::RaidKalli;
	}
}

void ADLRaidGameMode::BeginChamber(int32 ChamberIndex)
{
	ChamberIndex = FMath::Clamp(ChamberIndex, 0, ChamberCount - 1);
	if (ADLRaidGameState* GS = GetGameState<ADLRaidGameState>())
	{
		GS->SetRaidChamberIndex(ChamberIndex);
		GS->bChamberCleared = false;
	}
	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
	if (EncounterDirector)
	{
		EncounterDirector->BuildAndSpawnChamber(ChamberIndex);
	}
}

void ADLRaidGameMode::CompleteChamber()
{
	ADLRaidGameState* GS = GetGameState<ADLRaidGameState>();
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

	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
	}
}

void ADLRaidGameMode::AdvanceOrFinishRaid()
{
	ADLRaidGameState* GS = GetGameState<ADLRaidGameState>();
	if (!GS)
	{
		return;
	}

	const int32 Next = GS->GetRaidChamberIndex() + 1;
	if (Next >= ChamberCount)
	{
		if (UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>())
		{
			if (FDLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId))
			{
				Profile->Stats.RaidsCompleted += 1;
				Profiles->SaveActiveProfile();
			}
		}
		RequestExitToSocial();
		return;
	}

	if (UDLSceneRouter* Router = GetGameInstance()->GetSubsystem<UDLSceneRouter>())
	{
		Router->TravelToScene(EDLSceneId::Raid, Next);
	}
}
