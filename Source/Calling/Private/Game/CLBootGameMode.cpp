#include "Game/CLGameModeBase.h"
#include "Game/CLAbilitySmoke.h"
#include "Core/CLLog.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLGameInstance.h"
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
ACLBootGameMode::ACLBootGameMode()
{
	SceneId = ECLSceneId::Boot;
	GameStateClass = ACLBootGameState::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void ACLBootGameMode::StartPlay()
{
	Super::StartPlay();

	if (UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
	{
		Profiles->LoadAllProfiles();
		if (Profiles->ShouldAutoEnterSocial())
		{
			if (!Profiles->HasActiveProfile())
			{
				const TArray<FCLLocalProfile> All = Profiles->GetAllProfiles();
				if (All.Num() > 0)
				{
					Profiles->SelectProfile(All[0].ProfileId);
				}
			}
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					AutoEnterSocialTimer, this, &ACLBootGameMode::TravelToSocialDeferred, 0.15f, false);
			}
			return;
		}
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		TryShowBootProfileUI(PC);
	}
}

void ACLBootGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
	{
		if (Profiles->ShouldAutoEnterSocial())
		{
			return;
		}
	}
	TryShowBootProfileUI(NewPlayer);
}

void ACLBootGameMode::TravelToSocialDeferred()
{
	if (UCLSceneRouter* Router = GetGameInstance()->GetSubsystem<UCLSceneRouter>())
	{
		if (Router->GetCurrentScene() != ECLSceneId::Boot)
		{
			return;
		}
		Router->TravelToScene(ECLSceneId::Social);
	}
}

void ACLBootGameMode::TryShowBootProfileUI(APlayerController* PC)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (BootProfileWidget)
	{
		return;
	}

	BootProfileWidget = CreateWidget<UCLBootProfileWidget>(PC, UCLBootProfileWidget::StaticClass());
	if (BootProfileWidget)
	{
		BootProfileWidget->AddToViewport(10);
		BootProfileWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
		BootProfileWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
		BootProfileWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
	}
}
