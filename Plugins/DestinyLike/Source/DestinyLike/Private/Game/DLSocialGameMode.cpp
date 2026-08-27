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
ADLSocialGameMode::ADLSocialGameMode()
{
	SceneId = EDLSceneId::Social;
	GameStateClass = ADLSocialGameState::StaticClass();
}

void ADLSocialGameMode::EnsureSocialGreybox()
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

void ADLSocialGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	EnsureSocialGreybox();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	EnsureSocialGreybox();
}

AActor* ADLSocialGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	EnsureSocialGreybox();
	if (ADLGreyboxFloors* Floors = ADLGreyboxFloors::SpawnIfMissing(GetWorld(), EDLGreyboxLayout::SocialSquare))
	{
		return ADLGreyboxFloors::EnsurePlayerStart(GetWorld(), Floors->GetPlayerStartLocation());
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADLSocialGameMode::ShowSocialMarker()
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

	SocialMarker = CreateWidget<UDLSocialMarkerWidget>(PC, UDLSocialMarkerWidget::StaticClass());
	if (SocialMarker)
	{
		SocialMarker->AddToViewport(20);
	}
}

void ADLSocialGameMode::StartPlay()
{
	EnsureSocialGreybox();
	Super::StartPlay();
	EnsureSocialGreybox();
	ShowSocialMarker();

	FString ModeStr;
	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("DefaultSocialPvpMode"), ModeStr, GGameIni);
	EDLSocialPvpMode Mode = ModeStr.Equals(TEXT("Forced"), ESearchCase::IgnoreCase)
		? EDLSocialPvpMode::Forced : EDLSocialPvpMode::Optional;

	if (UDLSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UDLSessionSubsystem>())
	{
		if (Sessions->IsHosting())
		{
			Mode = Sessions->GetHostedSocialPvpMode();
		}
	}

	if (ADLGameStateBase* GS = GetGameState<ADLGameStateBase>())
	{
		GS->SetSocialPvpMode(Mode);
	}

	if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
	{
		Lobby->BeginOpenScene();
	}

	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		// Social doubles as hub: start in lobby-capable in-progress state.
		Activity->BeginInProgress();
	}

	if (DLShouldRunAbilitySmoke())
	{
		DLRunAbilitySmoke(GetWorld());
	}
}

void ADLSocialGameMode::SetHostSocialPvpMode(EDLSocialPvpMode Mode)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ADLGameStateBase* GS = GetGameState<ADLGameStateBase>())
	{
		GS->SetSocialPvpMode(Mode);
	}
}
