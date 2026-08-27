#include "Game/DLGameInstance.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLSessionHub.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLVaultSubsystem.h"
#include "Game/DLSessionSubsystem.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLErrorBoundary.h"
#include "Core/DLTickClock.h"
#include "Core/DLError.h"
#include "Loot/DLLootRulesService.h"
#include "Ability/DLAbilityCatalog.h"
#include "Misc/ConfigCacheIni.h"

void UDLGameInstance::Init()
{
	Super::Init();

	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("SocialMapName"), LastSocialMap, GGameIni);
	if (LastSocialMap.IsEmpty())
	{
		LastSocialMap = TEXT("/Game/Maps/DL_Social");
	}

	LootRulesService = NewObject<UDLLootRulesService>(this);
	if (LootRulesService && !LootRulesService->LoadConfigs())
	{
		UDLErrorBoundary::ReportStatic(this, FDLError::Make(
			EDLErrorKind::NonDeterministic,
			TEXT("loot_config"),
			TEXT("Loot configs failed at Init")));
	}

	UDLAbilityCatalog::Get(this);

	if (UDLTickSubsystem* Tick = GetSubsystem<UDLTickSubsystem>())
	{
		Tick->ReloadSettingsFromConfig();
	}

	if (UDLProfileSubsystem* Profiles = GetSubsystem<UDLProfileSubsystem>())
	{
		Profiles->LoadAllProfiles();
	}
}

void UDLGameInstance::Shutdown()
{
	if (UDLProfileSubsystem* Profiles = GetSubsystem<UDLProfileSubsystem>())
	{
		Profiles->SaveActiveProfile();
	}

	LootRulesService = nullptr;
	Super::Shutdown();
}

UDLProfileSubsystem* UDLGameInstance::GetProfileSubsystem() const
{
	return GetSubsystem<UDLProfileSubsystem>();
}

UDLVaultSubsystem* UDLGameInstance::GetVaultSubsystem() const
{
	return GetSubsystem<UDLVaultSubsystem>();
}

UDLSessionSubsystem* UDLGameInstance::GetSessionSubsystem() const
{
	return GetSubsystem<UDLSessionSubsystem>();
}

UDLSceneRouter* UDLGameInstance::GetSceneRouter() const
{
	return GetSubsystem<UDLSceneRouter>();
}

UDLTickSubsystem* UDLGameInstance::GetTickSubsystem() const
{
	return GetSubsystem<UDLTickSubsystem>();
}

UDLLootRulesService* UDLGameInstance::GetLootRulesService() const
{
	return LootRulesService;
}

UDLLobbySubsystem* UDLGameInstance::GetLobbySubsystem() const
{
	return GetSubsystem<UDLLobbySubsystem>();
}

void UDLGameInstance::SetLastSocialMap(const FString& MapName)
{
	if (!MapName.IsEmpty())
	{
		LastSocialMap = MapName;
	}
}
