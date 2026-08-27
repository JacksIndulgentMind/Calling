#include "Game/DLGameModeBase.h"
#include "Game/DLAbilitySmoke.h"
#include "Core/DLLog.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLVaultSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLGameInstance.h"
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
ADLBootGameMode::ADLBootGameMode()
{
	SceneId = EDLSceneId::Boot;
	GameStateClass = ADLBootGameState::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void ADLBootGameMode::StartPlay()
{
	Super::StartPlay();

	if (UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>())
	{
		Profiles->LoadAllProfiles();
		if (Profiles->ShouldAutoEnterSocial())
		{
			if (!Profiles->HasActiveProfile())
			{
				const TArray<FDLLocalProfile> All = Profiles->GetAllProfiles();
				if (All.Num() > 0)
				{
					Profiles->SelectProfile(All[0].ProfileId);
				}
			}
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					AutoEnterSocialTimer, this, &ADLBootGameMode::TravelToSocialDeferred, 0.15f, false);
			}
			return;
		}
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		TryShowBootProfileUI(PC);
	}
}

void ADLBootGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (UDLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UDLProfileSubsystem>())
	{
		if (Profiles->ShouldAutoEnterSocial())
		{
			return;
		}
	}
	TryShowBootProfileUI(NewPlayer);
}

void ADLBootGameMode::TravelToSocialDeferred()
{
	if (UDLSceneRouter* Router = GetGameInstance()->GetSubsystem<UDLSceneRouter>())
	{
		if (Router->GetCurrentScene() != EDLSceneId::Boot)
		{
			return;
		}
		Router->TravelToScene(EDLSceneId::Social);
	}
}

void ADLBootGameMode::TryShowBootProfileUI(APlayerController* PC)
{
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (BootProfileWidget)
	{
		return;
	}

	BootProfileWidget = CreateWidget<UDLBootProfileWidget>(PC, UDLBootProfileWidget::StaticClass());
	if (BootProfileWidget)
	{
		BootProfileWidget->AddToViewport(10);
		BootProfileWidget->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
		BootProfileWidget->SetAlignmentInViewport(FVector2D(0.f, 0.f));
		BootProfileWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
	}
}
