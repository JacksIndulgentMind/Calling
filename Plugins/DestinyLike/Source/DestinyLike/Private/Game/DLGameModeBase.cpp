#include "Game/DLGameModeBase.h"
#include "Game/DLAbilitySmoke.h"
#include "Core/DLLog.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLVaultSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLGameInstance.h"
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
ADLGameModeBase::ADLGameModeBase()
{
	GameStateClass = ADLGameStateBase::StaticClass();
	PlayerControllerClass = ADLPlayerController::StaticClass();
	DefaultPawnClass = ADLPlayerCharacter::StaticClass();
	DefaultCombatPawnClass = ADLPlayerCharacter::StaticClass();
	bUseSeamlessTravel = false;
}

void ADLGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
}

UClass* ADLGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	(void)InController;
	EDLClassId ClassId = EDLClassId::Vanguard;
	if (UDLGameInstance* GI = Cast<UDLGameInstance>(GetGameInstance()))
	{
		if (UDLProfileSubsystem* Profiles = GI->GetProfileSubsystem())
		{
			if (Profiles->HasActiveProfile())
			{
				ClassId = Profiles->GetActiveProfile().Character.ClassId;
			}
		}
	}
	if (UDLAbilityCatalog* Catalog = UDLAbilityCatalog::Get(this))
	{
		if (UClass* PawnClass = Catalog->ResolvePawnClass(ClassId).Get())
		{
			return PawnClass;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADLGameModeBase::StartPlay()
{
	Super::StartPlay();
	ADLGreyboxFloors::ApplyVoidWorldSettings(GetWorld());
	SyncSceneToGameState();

	if (UDLSceneRouter* Router = GetGameInstance()->GetSubsystem<UDLSceneRouter>())
	{
		Router->SetCurrentScene(SceneId);
		if (SceneId == EDLSceneId::Social)
		{
			const FString MapPath = GetWorld() && GetWorld()->GetOutermost()
				? GetWorld()->GetOutermost()->GetName()
				: FString();
			Router->RememberSocialMap(MapPath);
		}
	}

	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ADLGameModeBase::HandleLobbyGo()
{
	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginInProgress();
	}
}

void ADLGameModeBase::RequestRespawn(AController* Player)
{
	if (bRespawnInProgress || !Player || SceneId == EDLSceneId::Boot)
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
		if (ADLPlayerCharacter* DL = Cast<ADLPlayerCharacter>(Pawn))
		{
			DL->ClearAgentIntent();
			if (UDLHealthShieldComponent* HS = DL->GetHealthShield())
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

void ADLGameModeBase::SyncSceneToGameState() const
{
	if (ADLGameStateBase* GS = GetGameState<ADLGameStateBase>())
	{
		GS->SetSceneId(SceneId);
	}
}

UDLActivityStateComponent* ADLGameModeBase::GetActivityState() const
{
	if (const ADLGameStateBase* GS = GetGameState<ADLGameStateBase>())
	{
		return GS->GetActivityState();
	}
	return nullptr;
}

void ADLGameModeBase::RequestExitToSocial()
{
	if (UDLSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UDLSessionSubsystem>())
	{
		Sessions->DestroySession();
	}
	if (UDLActivityStateComponent* Activity = GetActivityState())
	{
		Activity->BeginReturning();
	}
	if (UDLSceneRouter* Router = GetGameInstance()->GetSubsystem<UDLSceneRouter>())
	{
		Router->ExitActivityToSocial();
	}
}

void ADLGameModeBase::AwardDropFromTable(FName TableId, AController* ToController)
{
	UDLGameInstance* GI = Cast<UDLGameInstance>(GetGameInstance());
	if (!GI || !GI->GetLootRulesService() || !GI->GetVaultSubsystem())
	{
		return;
	}

	// Vault is local-profile scoped; award on owning client/listen host for that controller.
	if (ToController && ToController->IsLocalController())
	{
		FDLItemInstance Item;
		if (GI->GetLootRulesService()->RollDrop(TableId, Item))
		{
			if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
			{
				Item.RealmId = Lobby->GetLootRealmId();
			}
			GI->GetVaultSubsystem()->DepositItem(Item);
		}
	}
}
