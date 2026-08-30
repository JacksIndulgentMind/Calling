#include "Game/CLGameModeBase.h"
#include "Game/CLAbilitySmoke.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSessionSubsystem.h"
#include "Game/CLGameModeCatalog.h"
#include "Game/CLErrorBoundary.h"
#include "Loot/CLLootRulesService.h"
#include "AI/CLEncounterDirector.h"
#include "AI/CLPracticeDummy.h"
#include "AI/CLTaskMarker.h"
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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ACLPvpGameMode::PlaceSpawnsFromMarkers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (const ACLTaskMarker* Red = ACLTaskMarker::FindByTag(World, FName(TEXT("spawn.player.red"))))
	{
		ACLGreyboxFloors::EnsureTaggedPlayerStart(World, FName(TEXT("Red")), Red->GetActorLocation(), FRotator(0.f, 0.f, 0.f));
	}
	if (const ACLTaskMarker* Blue = ACLTaskMarker::FindByTag(World, FName(TEXT("spawn.player.blue"))))
	{
		ACLGreyboxFloors::EnsureTaggedPlayerStart(World, FName(TEXT("Blue")), Blue->GetActorLocation(), FRotator(0.f, 180.f, 0.f));
	}
}

void ACLPvpGameMode::BindShrineClash()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = GetGameInstance();
	if (!World || !GI || bModeBound)
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
	FName ModeId = FName(TEXT("shrine_clash"));
	if (Lobby && Lobby->GetInvoice() && !Lobby->GetInvoice()->GameModeId.IsNone())
	{
		ModeId = Lobby->GetInvoice()->GameModeId;
	}
	Catalog->ApplyMarkerTags(World, CatalogMapId);
	PlaceSpawnsFromMarkers();
	const FCLStatus Status = Catalog->Validate(World, CatalogMapId, ModeId);
	if (!Status.IsOk())
	{
		UCLErrorBoundary::ReportStatic(this, Status.Error);
		return;
	}
	if (const FCLGameModeDef* Mode = Catalog->FindMode(ModeId))
	{
		TeamFinalBlows = Mode->TeamFinalBlows;
		OccupyTag = Mode->OccupyTag;
		RotateSeconds = Mode->RotateSeconds;
		bStealIfTenWithoutShrine = Mode->bStealIfTenWithoutShrine;
		bFailIfEitherTeamKillsZero = Mode->bFailIfEitherTeamKillsZero;
		FailTimeoutSeconds = Mode->FailTimeoutSeconds;
	}
	TArray<ACLTaskMarker*> Shrines;
	ACLTaskMarker::CollectByTag(World, OccupyTag, Shrines);
	ShrineMarkerIds.Reset();
	for (const ACLTaskMarker* M : Shrines)
	{
		if (M)
		{
			ShrineMarkerIds.Add(M->Id);
		}
	}
	ShrineIndex = 0;
	MatchStartSeconds = FPlatformTime::Seconds();
	NextRotateSeconds = MatchStartSeconds + RotateSeconds;
	bModeBound = true;
	bModeFinished = false;
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetModeOutcome(TEXT("in_progress"), TEXT(""), TEXT(""));
		if (ShrineMarkerIds.Num() > 0)
		{
			GS->SetLiveShrine(ShrineMarkerIds[0]);
		}
	}
	UE_LOG(LogCalling, Display, TEXT("Pvp shrine_clash bound shrines=%d blows=%d rotate=%.0f failT=%.0f"),
		ShrineMarkerIds.Num(), TeamFinalBlows, RotateSeconds, FailTimeoutSeconds);
}

void ACLPvpGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::PvpThreeLane);
}

void ACLPvpGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->EnsureNetHumanSeat(NewPlayer);
	}
}

AActor* ACLPvpGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::PvpThreeLane);
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLGameModeCatalog* Catalog = GI->GetSubsystem<UCLGameModeCatalog>())
		{
			Catalog->ApplyMarkerTags(GetWorld(), CatalogMapId);
			PlaceSpawnsFromMarkers();
		}
	}
	FName Tag = FName(TEXT("Red"));
	FName SpawnTag = FName(TEXT("spawn.player.red"));
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		if (const UCLParticipantSeat* Seat = Lobby->FindSeatForController(Player))
		{
			if (Seat->GetTeam() == ECLPvpTeam::Blue)
			{
				Tag = FName(TEXT("Blue"));
				SpawnTag = FName(TEXT("spawn.player.blue"));
			}
		}
		else if (const APlayerController* PC = Cast<APlayerController>(Player))
		{
			if (!PC->IsLocalController())
			{
				Tag = FName(TEXT("Blue"));
				SpawnTag = FName(TEXT("spawn.player.blue"));
			}
		}
	}
	if (const ACLTaskMarker* Marker = ACLTaskMarker::FindByTag(GetWorld(), SpawnTag))
	{
		APlayerStart* Start = ACLGreyboxFloors::EnsureTaggedPlayerStart(
			GetWorld(), Tag, Marker->GetActorLocation(),
			Tag == FName(TEXT("Blue")) ? FRotator(0.f, 180.f, 0.f) : FRotator::ZeroRotator);
		if (Start)
		{
			return Start;
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
	BindShrineClash();
	SetActorTickEnabled(true);
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

void ACLPvpGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickMode(DeltaSeconds);
}

void ACLPvpGameMode::HandleLobbyGo()
{
	StartMatchFromLobby();
}

void ACLPvpGameMode::StartMatchFromLobby()
{
	bModeFinished = false;
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->ClearMatchEvents();
		GS->SetModeOutcome(TEXT("in_progress"), TEXT(""), TEXT(""));
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
	if (!bModeBound)
	{
		BindShrineClash();
	}
	MatchStartSeconds = FPlatformTime::Seconds();
	NextRotateSeconds = MatchStartSeconds + RotateSeconds;
}

ECLPvpTeam ACLPvpGameMode::OccupantOfLiveShrine() const
{
	UWorld* World = GetWorld();
	ACLGameStateBase* GS = GetGameState<ACLGameStateBase>();
	if (!World || !GS || GS->GetLiveShrine().IsNone())
	{
		return ECLPvpTeam::Unassigned;
	}
	const ACLTaskMarker* Marker = ACLTaskMarker::FindById(World, GS->GetLiveShrine());
	if (!Marker)
	{
		return ECLPvpTeam::Unassigned;
	}
	const float Band = FMath::Max(Marker->ZoneRadiusCm, 150.f);
	UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	ECLPvpTeam Found = ECLPvpTeam::Unassigned;
	int32 FoundCount = 0;
	for (TActorIterator<ACLPlayerCharacter> It(World); It; ++It)
	{
		ACLPlayerCharacter* Pawn = *It;
		if (!Pawn || !Pawn->IsCombatAlive())
		{
			continue;
		}
		const FVector Delta = Pawn->GetActorLocation() - Marker->GetActorLocation();
		if (FVector(Delta.X, Delta.Y, 0.f).Size() > Band)
		{
			continue;
		}
		ECLPvpTeam Team = ECLPvpTeam::Unassigned;
		if (Lobby)
		{
			if (const UCLParticipantSeat* Seat = Lobby->FindSeatForController(Pawn->GetController()))
			{
				Team = Seat->GetTeam();
			}
		}
		if (Team == ECLPvpTeam::Red || Team == ECLPvpTeam::Blue)
		{
			if (Found == ECLPvpTeam::Unassigned)
			{
				Found = Team;
			}
			else if (Found != Team)
			{
				return ECLPvpTeam::Unassigned;
			}
			++FoundCount;
		}
	}
	return FoundCount > 0 ? Found : ECLPvpTeam::Unassigned;
}

void ACLPvpGameMode::SampleShrineOccupancy()
{
	ACLGameStateBase* GS = GetGameState<ACLGameStateBase>();
	if (!GS)
	{
		return;
	}
	const ECLPvpTeam Occ = OccupantOfLiveShrine();
	if (Occ == ECLPvpTeam::Red)
	{
		GS->SetShrineHeld(ECLPvpTeam::Red, true);
	}
	else if (Occ == ECLPvpTeam::Blue)
	{
		GS->SetShrineHeld(ECLPvpTeam::Blue, true);
	}
}

void ACLPvpGameMode::RotateLiveShrine()
{
	if (ShrineMarkerIds.Num() == 0)
	{
		return;
	}
	ShrineIndex = (ShrineIndex + 1) % ShrineMarkerIds.Num();
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetLiveShrine(ShrineMarkerIds[ShrineIndex]);
	}
	NextRotateSeconds = FPlatformTime::Seconds() + RotateSeconds;
}

void ACLPvpGameMode::FailBook(const FString& Reason)
{
	FinishMode(TEXT("fail"), TEXT(""), Reason);
}

void ACLPvpGameMode::FinishMode(const FString& Result, const FString& Winner, const FString& FailReason)
{
	if (bModeFinished)
	{
		return;
	}
	bModeFinished = true;
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetModeOutcome(Result, Winner, FailReason);
		FCLMatchEvent E;
		E.Code = Result == TEXT("winner") ? TEXT("mode_winner") : TEXT("mode_fail");
		E.Detail = FailReason.IsEmpty() ? Result : FailReason;
		E.Book = Winner;
		E.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		GS->AppendMatchEvent(E);
	}
	UE_LOG(LogCalling, Display, TEXT("Pvp modeResult=%s winner=%s fail=%s"), *Result, *Winner, *FailReason);
	if (Result == TEXT("winner"))
	{
		EndMatchAndAward();
	}
	else if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginResults();
	}
}

void ACLPvpGameMode::TryResolveMatch()
{
	ACLGameStateBase* GS = GetGameState<ACLGameStateBase>();
	if (!GS || bModeFinished)
	{
		return;
	}
	const int32 RedK = GS->GetTeamAKills();
	const int32 BlueK = GS->GetTeamBKills();
	const bool bEitherZero = (RedK == 0 || BlueK == 0);
	const bool bRedTen = RedK >= TeamFinalBlows;
	const bool bBlueTen = BlueK >= TeamFinalBlows;

	auto FailZero = [&]()
	{
		if (bFailIfEitherTeamKillsZero && bEitherZero)
		{
			FinishMode(TEXT("fail"), TEXT(""), TEXT("zero_kills"));
			return true;
		}
		return false;
	};

	if (bRedTen || bBlueTen)
	{
		if (FailZero())
		{
			return;
		}
		const bool bRedCredit = GS->GetShrineHeldRed();
		const bool bBlueCredit = GS->GetShrineHeldBlue();
		if (bRedTen && bRedCredit)
		{
			FinishMode(TEXT("winner"), TEXT("red"), TEXT(""));
			return;
		}
		if (bBlueTen && bBlueCredit)
		{
			FinishMode(TEXT("winner"), TEXT("blue"), TEXT(""));
			return;
		}
		if (bStealIfTenWithoutShrine)
		{
			const ECLPvpTeam Occ = OccupantOfLiveShrine();
			if (Occ == ECLPvpTeam::Red)
			{
				FinishMode(TEXT("winner"), TEXT("red"), TEXT("steal"));
				return;
			}
			if (Occ == ECLPvpTeam::Blue)
			{
				FinishMode(TEXT("winner"), TEXT("blue"), TEXT("steal"));
				return;
			}
		}
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (FailTimeoutSeconds > 0.f && (Now - MatchStartSeconds) >= FailTimeoutSeconds)
	{
		if (FailZero() || bEitherZero)
		{
			if (!bModeFinished)
			{
				FinishMode(TEXT("fail"), TEXT(""), TEXT("timeout"));
			}
			return;
		}
		FinishMode(TEXT("fail"), TEXT(""), TEXT("timeout"));
	}
}

void ACLPvpGameMode::TickMode(float DeltaSeconds)
{
	(void)DeltaSeconds;
	if (!bModeBound || bModeFinished || !HasAuthority())
	{
		return;
	}
	SampleShrineOccupancy();
	if (FPlatformTime::Seconds() >= NextRotateSeconds)
	{
		RotateLiveShrine();
	}
	TryResolveMatch();
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
