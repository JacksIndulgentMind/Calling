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
ACLSocialGameMode::ACLSocialGameMode()
{
	SceneId = ECLSceneId::Social;
	GameStateClass = ACLSocialGameState::StaticClass();
}

void ACLSocialGameMode::EnsureSocialGreybox()
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

void ACLSocialGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	EnsureSocialGreybox();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsureSocialGreybox();
}

AActor* ACLSocialGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	EnsureSocialGreybox();
	if (ACLGreyboxFloors* Floors = ACLGreyboxFloors::SpawnIfMissing(GetWorld(), ECLGreyboxLayout::SocialSquare))
	{
		return ACLGreyboxFloors::EnsurePlayerStart(GetWorld(), Floors->GetPlayerStartLocation());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ACLSocialGameMode::ShowSocialMarker()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(99201, 60.f, FColor::White, TEXT("Social"));
	}

	if (SocialMarker)
	{
		return;
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	SocialMarker = CreateWidget<UCLSocialMarkerWidget>(PC, UCLSocialMarkerWidget::StaticClass());
	if (SocialMarker)
	{
		SocialMarker->AddToViewport(20);
	}
}

void ACLSocialGameMode::StartPlay()
{
	EnsureSocialGreybox();
	Super::StartPlay();
	EnsureSocialGreybox();
	ShowSocialMarker();

	FString ModeStr;
	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("DefaultSocialPvpMode"), ModeStr, GGameIni);
	ECLSocialPvpMode Mode = ModeStr.Equals(TEXT("Forced"), ESearchCase::IgnoreCase)
		? ECLSocialPvpMode::Forced : ECLSocialPvpMode::Optional;

	if (UCLSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCLSessionSubsystem>())
	{
		if (Sessions->IsHosting())
		{
			Mode = Sessions->GetHostedSocialPvpMode();
		}
	}

	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetSocialPvpMode(Mode);
	}

	if (UCLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UCLLobbySubsystem>())
	{
		Lobby->BeginOpenScene();
	}

	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		// Social doubles as hub: start in lobby-capable in-progress state.
		Activity->BeginInProgress();
	}

	if (CLShouldRunAbilitySmoke())
	{
		CLRunAbilitySmoke(GetWorld());
	}
}

void ACLSocialGameMode::SetHostSocialPvpMode(ECLSocialPvpMode Mode)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetSocialPvpMode(Mode);
	}
}
