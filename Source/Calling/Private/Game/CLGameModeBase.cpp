#include "Game/CLGameModeBase.h"
#include "Game/CLAbilitySmoke.h"
#include "Core/CLLog.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLGameInstance.h"
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
ACLGameModeBase::ACLGameModeBase()
{
	GameStateClass = ACLGameStateBase::StaticClass();
	PlayerControllerClass = ACLPlayerController::StaticClass();
	DefaultPawnClass = ACLPlayerCharacter::StaticClass();
	DefaultCombatPawnClass = ACLPlayerCharacter::StaticClass();
	bUseSeamlessTravel = false;
}

void ACLGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
}

UClass* ACLGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	(void)InController;
	ECLClassId ClassId = ECLClassId::Vanguard;
	if (UCLGameInstance* GI = Cast<UCLGameInstance>(GetGameInstance()))
	{
		if (UCLProfileSubsystem* Profiles = GI->GetProfileSubsystem())
		{
			if (Profiles->HasActiveProfile())
			{
				ClassId = Profiles->GetActiveProfile().Character.ClassId;
			}
		}
	}
	if (UCLAbilityCatalog* Catalog = UCLAbilityCatalog::Get(this))
	{
		if (UClass* PawnClass = Catalog->ResolvePawnClass(ClassId).Get())
		{
			return PawnClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ACLGameModeBase::StartPlay()
{
	Super::StartPlay();
	ACLGreyboxFloors::ApplyVoidWorldSettings(GetWorld());
	SyncSceneToGameState();

	if (UCLSceneRouter* Router = GetGameInstance()->GetSubsystem<UCLSceneRouter>())
	{
		Router->SetCurrentScene(SceneId);
		if (SceneId == ECLSceneId::Social)
		{
			const FString MapPath = GetWorld() && GetWorld()->GetOutermost()
				? GetWorld()->GetOutermost()->GetName()
				: FString();
			Router->RememberSocialMap(MapPath);
		}
	}

	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ACLGameModeBase::HandleLobbyGo()
{
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ACLGameModeBase::RequestRespawn(AController* Player)
{
	if (bRespawnInProgress || !Player || SceneId == ECLSceneId::Boot)
	{
		return;
	}
	bRespawnInProgress = true;

	AActor* Start = ChoosePlayerStart(Player);
	const FVector Loc = Start ? Start->GetActorLocation() : FVector(0.f, 0.f, 130.f);
	const FRotator Rot = Start ? Start->GetActorRotation() : FRotator::ZeroRotator;

	if (APawn* Pawn = Player->GetPawn())
	{
		Pawn->TeleportTo(Loc, Rot, false, true);
		if (ACharacter* Char = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				Move->StopMovementImmediately();
			}
		}
		if (ACLPlayerCharacter* DL = Cast<ACLPlayerCharacter>(Pawn))
		{
			DL->ClearAgentIntent();
			if (UCLHealthShieldComponent* HS = DL->GetHealthShield())
			{
				HS->ResetToFull();
			}
			DL->NotifyRespawned();
		}
		if (APlayerController* PC = Cast<APlayerController>(Player))
		{
			PC->SetControlRotation(Rot);
		}
		bRespawnInProgress = false;
		return;
	}

	RestartPlayer(Player);
	if (APlayerController* PC = Cast<APlayerController>(Player))
	{
		PC->SetControlRotation(Rot);
	}
	bRespawnInProgress = false;
}

void ACLGameModeBase::SyncSceneToGameState() const
{
	if (ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		GS->SetSceneId(SceneId);
	}
}

UCLActivityStateComponent* ACLGameModeBase::GetActivityState() const
{
	if (const ACLGameStateBase* GS = GetGameState<ACLGameStateBase>())
	{
		return GS->GetActivityState();
	}
	return nullptr;
}

void ACLGameModeBase::RequestExitToSocial()
{
	if (UCLSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCLSessionSubsystem>())
	{
		Sessions->DestroySession();
	}
	if (UCLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginReturning();
	}
	if (UCLSceneRouter* Router = GetGameInstance()->GetSubsystem<UCLSceneRouter>())
	{
		Router->ExitActivityToSocial();
	}
}

void ACLGameModeBase::AwardDropFromTable(FName TableId, AController* ToController)
{
	UCLGameInstance* GI = Cast<UCLGameInstance>(GetGameInstance());
	if (!GI || !GI->GetLootRulesService() || !GI->GetVaultSubsystem())
	{
		return;
	}

	// Vault is local-profile scoped; award on owning client/listen host for that controller.
	if (ToController && ToController->IsLocalController())
	{
		FCLItemInstance Item;
		if (GI->GetLootRulesService()->RollDrop(TableId, Item))
		{
			if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
			{
				Item.RealmId = Lobby->GetLootRealmId();
			}
			GI->GetVaultSubsystem()->DepositItem(Item);
		}
	}
}
