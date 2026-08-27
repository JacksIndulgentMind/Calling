#include "Game/CLGameInstance.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLSessionHub.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLSessionSubsystem.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLErrorBoundary.h"
#include "Core/CLTickClock.h"
#include "Core/CLError.h"
#include "Loot/CLLootRulesService.h"
#include "Ability/CLAbilityCatalog.h"
#include "Misc/ConfigCacheIni.h"

void UCLGameInstance::Init()
{
	Super::Init();

	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("SocialMapName"), LastSocialMap, GGameIni);
	if (LastSocialMap.IsEmpty())
	{
		LastSocialMap = TEXT("/Game/Maps/CL_Social");
	}

	LootRulesService = NewObject<UCLLootRulesService>(this);
	if (LootRulesService && !LootRulesService->LoadConfigs())
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("loot_config"),
			TEXT("Loot configs failed at Init")));
	}

	UCLAbilityCatalog::Get(this);

	if (UCLTickSubsystem* Tick = GetSubsystem<UCLTickSubsystem>())
	{
		Tick->ReloadSettingsFromConfig();
	}

	if (UCLProfileSubsystem* Profiles = GetSubsystem<UCLProfileSubsystem>())
	{
		Profiles->LoadAllProfiles();
	}
}

void UCLGameInstance::Shutdown()
{
	if (UCLProfileSubsystem* Profiles = GetSubsystem<UCLProfileSubsystem>())
	{
		Profiles->SaveActiveProfile();
	}

	LootRulesService = nullptr;
	Super::Shutdown();
}

UCLProfileSubsystem* UCLGameInstance::GetProfileSubsystem() const
{
	return GetSubsystem<UCLProfileSubsystem>();
}

UCLVaultSubsystem* UCLGameInstance::GetVaultSubsystem() const
{
	return GetSubsystem<UCLVaultSubsystem>();
}

UCLSessionSubsystem* UCLGameInstance::GetSessionSubsystem() const
{
	return GetSubsystem<UCLSessionSubsystem>();
}

UCLSceneRouter* UCLGameInstance::GetSceneRouter() const
{
	return GetSubsystem<UCLSceneRouter>();
}

UCLTickSubsystem* UCLGameInstance::GetTickSubsystem() const
{
	return GetSubsystem<UCLTickSubsystem>();
}

UCLLootRulesService* UCLGameInstance::GetLootRulesService() const
{
	return LootRulesService;
}

UCLLobbySubsystem* UCLGameInstance::GetLobbySubsystem() const
{
	return GetSubsystem<UCLLobbySubsystem>();
}

void UCLGameInstance::SetLastSocialMap(const FString& MapName)
{
	if (!MapName.IsEmpty())
	{
		LastSocialMap = MapName;
	}
}
