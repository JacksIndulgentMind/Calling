#include "Game/CLSceneRouter.h"
#include "Game/CLGameInstance.h"
#include "Game/CLGameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UCLSceneRouter::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadMapNamesFromConfig();
	LastSocialMapName = SocialMapName;
}

void UCLSceneRouter::ReloadMapNamesFromConfig()
{
	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("BootMapName"), BootMapName, GGameIni);
	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("SocialMapName"), SocialMapName, GGameIni);
	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("PvpMapName"), PvpMapName, GGameIni);
	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("PracticeMapName"), PracticeMapName, GGameIni);

	if (BootMapName.IsEmpty()) BootMapName = TEXT("/Game/Maps/CL_Boot");
	if (SocialMapName.IsEmpty()) SocialMapName = TEXT("/Game/Maps/CL_Social");
	if (PvpMapName.IsEmpty()) PvpMapName = TEXT("/Game/Maps/CL_PvpArena");
	if (PracticeMapName.IsEmpty()) PracticeMapName = TEXT("/Game/Maps/CL_Practice");

	FString RaidCsv;
	GConfig->GetString(TEXT("/Script/Calling.CLSceneSettings"), TEXT("RaidChamberMapNames"), RaidCsv, GGameIni);
	RaidChamberMapNames.Reset();
	RaidCsv.ParseIntoArray(RaidChamberMapNames, TEXT(","), true);
	if (RaidChamberMapNames.Num() == 0)
	{
		RaidChamberMapNames = {
			TEXT("/Game/Maps/CL_Raid_01"),
			TEXT("/Game/Maps/CL_Raid_02"),
			TEXT("/Game/Maps/CL_Raid_03"),
			TEXT("/Game/Maps/CL_Raid_04")
		};
	}
}

FString UCLSceneRouter::GetMapNameForScene(ECLSceneId Scene, int32 RaidChamberIndex) const
{
	switch (Scene)
	{
	case ECLSceneId::Boot: return BootMapName;
	case ECLSceneId::Social: return LastSocialMapName.IsEmpty() ? SocialMapName : LastSocialMapName;
	case ECLSceneId::Composer: return SocialMapName;
	case ECLSceneId::Pvp: return PvpMapName;
	case ECLSceneId::Practice: return PracticeMapName;
	case ECLSceneId::Raid:
		if (RaidChamberMapNames.IsValidIndex(RaidChamberIndex))
		{
			return RaidChamberMapNames[RaidChamberIndex];
		}
		return RaidChamberMapNames.Num() > 0 ? RaidChamberMapNames[0] : TEXT("/Game/Maps/CL_Raid_01");
	default: return SocialMapName;
	}
}

void UCLSceneRouter::RememberSocialMap(const FString& MapName)
{
	if (!MapName.IsEmpty())
	{
		LastSocialMapName = MapName;
		if (UCLGameInstance* GI = Cast<UCLGameInstance>(GetGameInstance()))
		{
			GI->SetLastSocialMap(MapName);
		}
	}
}

FString UCLSceneRouter::GameModePathForScene(ECLSceneId Scene)
{
	switch (Scene)
	{
	case ECLSceneId::Boot: return TEXT("/Script/Calling.CLBootGameMode");
	case ECLSceneId::Social: return TEXT("/Script/Calling.CLSocialGameMode");
	case ECLSceneId::Composer: return TEXT("/Script/Calling.CLComposerGameMode");
	case ECLSceneId::Pvp: return TEXT("/Script/Calling.CLPvpGameMode");
	case ECLSceneId::Practice: return TEXT("/Script/Calling.CLPracticeGameMode");
	case ECLSceneId::Raid: return TEXT("/Script/Calling.CLRaidGameMode");
	default: return TEXT("/Script/Calling.CLSocialGameMode");
	}
}

void UCLSceneRouter::SoftTravel(const FString& MapName, ECLSceneId Scene)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString Options = FString::Printf(
		TEXT("game=%s?cl=%d?NoSeamlessTravel"),
		*GameModePathForScene(Scene),
		static_cast<int32>(Scene));
	const FString URL = FString::Printf(TEXT("%s?%s"), *MapName, *Options);

	FString CurrentShort;
	if (const UPackage* Pkg = World->GetOutermost())
	{
		CurrentShort = FPackageName::GetShortName(Pkg->GetName());
	}
	const FString DestShort = FPackageName::GetShortName(MapName);
	const bool bSameMap = !CurrentShort.IsEmpty() && CurrentShort.Equals(DestShort, ESearchCase::IgnoreCase);
	if (const ACLGameModeBase* GM = Cast<ACLGameModeBase>(World->GetAuthGameMode()))
	{
		if (GM->GetSceneId() == Scene)
		{
			return;
		}
	}

	// Same .umap + different GameMode (Social ↔ Composer): ServerTravel often no-ops. OpenLevel reloads.
	if (bSameMap && World->HasBegunPlay())
	{
		UGameplayStatics::OpenLevel(World, FName(*MapName), true, Options);
		return;
	}

	if (World->IsPlayInEditor() || World->GetNetMode() != NM_Client)
	{
		World->ServerTravel(URL, true);
		return;
	}

	UGameplayStatics::OpenLevel(World, FName(*MapName), true, Options);
}

void UCLSceneRouter::TravelToScene(ECLSceneId Scene, int32 RaidChamberIndex)
{
	CurrentScene = Scene;
	if (Scene == ECLSceneId::Social)
	{
		RememberSocialMap(GetMapNameForScene(ECLSceneId::Social));
	}

	UWorld* World = GetWorld();
	const FString MapName = GetMapNameForScene(Scene, RaidChamberIndex);
	if (World && World->HasBegunPlay())
	{
		TWeakObjectPtr<UCLSceneRouter> WeakThis(this);
		World->GetTimerManager().SetTimer(
			DeferredTravelTimer,
			FTimerDelegate::CreateLambda([WeakThis, MapName, Scene]()
			{
				if (UCLSceneRouter* Router = WeakThis.Get())
				{
					Router->TravelDeferred(MapName, Scene);
				}
			}),
			0.05f,
			false);
		return;
	}

	SoftTravel(MapName, Scene);
}

void UCLSceneRouter::TravelDeferred(const FString& MapName, ECLSceneId Scene)
{
	SoftTravel(MapName, Scene);
}

void UCLSceneRouter::ExitActivityToSocial()
{
	CurrentScene = ECLSceneId::Social;
	SoftTravel(GetMapNameForScene(ECLSceneId::Social), ECLSceneId::Social);
}
