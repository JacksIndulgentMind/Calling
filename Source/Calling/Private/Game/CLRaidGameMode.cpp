#include "Game/CLGameModeBase.h"
#include "Game/CLAbilitySmoke.h"
#include "Core/CLLog.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLGameModeCatalog.h"
#include "Game/CLEncounterRules.h"
#include "Game/CLErrorBoundary.h"
#include "Core/CLError.h"
#include "AI/CLTaskMarker.h"
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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACLRaidGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::RaidObelisk);
}

void ACLRaidGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ACLRaidGameMode::StartPlay()
{
	Super::StartPlay();
	for (TActorIterator<ACLGreyboxFloors> It(GetWorld()); It; ++It)
	{
		It->RebuildNavigation();
	}
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
	BindObeliskRaid();
}

void ACLRaidGameMode::BindObeliskRaid()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	if (!World || !GI)
	{
		return;
	}
	UCLGameModeCatalog* Catalog = GI->GetSubsystem<UCLGameModeCatalog>();
	UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>();
	if (!Catalog)
	{
		return;
	}
	Catalog->LoadFiles();
	FName ModeId = FName(TEXT("obelisk_raid"));
	if (Lobby && Lobby->GetInvoice() && !Lobby->GetInvoice()->GameModeId.IsNone())
	{
		ModeId = Lobby->GetInvoice()->GameModeId;
	}
	Catalog->ApplyMarkerTags(World, CatalogMapId);
	if (const ACLTaskMarker* Spawn = ACLTaskMarker::FindByTag(World, FName(TEXT("spawn.player"))))
	{
		ACLGreyboxFloors::EnsurePlayerStart(World, Spawn->GetActorLocation());
	}
	const FCLStatus Status = Catalog->Validate(World, CatalogMapId, ModeId);
	if (!Status.IsOk())
	{
		UCLErrorBoundary::ReportStatic(this, Status.Error);
		return;
	}
	TArray<const FCLWaveHoldEncounter*> Holds;
	if (const FCLGameModeDef* Mode = Catalog->FindMode(ModeId))
	{
		Mode->CollectWaveHold(Holds);
	}
	if (Holds.Num() == 0)
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::Logic, TEXT("raid_missing_waveHold"), ModeId.ToString()));
		return;
	}
	bModeFinished = false;
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetModeOutcome(TEXT("in_progress"), TEXT(""), TEXT(""));
	}
	if (EncounterDirector)
	{
		EncounterDirector->BindWaveHold(Holds);
		EncounterDirector->BeginFirstEncounter();
	}
}

void ACLRaidGameMode::NotifyEncounterBegin(int32 InEncounterIndex)
{
	BeginChamber(InEncounterIndex);
}

void ACLRaidGameMode::NotifyEncounterComplete(int32 InEncounterIndex, FName OpensMarker)
{
	if (ACLRaidGameState* GS = GetGameState<ACLRaidGameState>())
	{
		GS->bChamberCleared = true;
		GS->ChambersCompleted = InEncounterIndex + 1;
	}
	const FName TableId(*FString::Printf(TEXT("raid_chamber_%d"), InEncounterIndex + 1));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AwardDropFromTable(TableId, It->Get());
	}
	if (!OpensMarker.IsNone())
	{
		for (TActorIterator<ACLGreyboxFloors> It(GetWorld()); It; ++It)
		{
			It->OpenDoor(OpensMarker);
		}
	}
}

void ACLRaidGameMode::FailBook(const FString& Reason)
{
	if (bModeFinished)
	{
		return;
	}
	bModeFinished = true;
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetModeOutcome(TEXT("fail"), TEXT(""), Reason);
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
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
	if (bModeFinished)
	{
		return;
	}
	bModeFinished = true;
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetModeOutcome(TEXT("winner"), TEXT(""), TEXT(""));
	}
	if (UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
	{
		if (FCLLocalProfile* Profile = Profiles->FindProfileMutable(Profiles->GetActiveProfile().ProfileId))
		{
			Profile->Stats.RaidsCompleted += 1;
			Profiles->SaveActiveProfile();
		}
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
	}
	RequestExitToSocial();
}
