#include "Game/DLSceneRouter.h"
#include "Game/DLGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UDLSceneRouter::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadMapNamesFromConfig();
	LastSocialMapName = SocialMapName;
}

void UDLSceneRouter::ReloadMapNamesFromConfig()
{
	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("BootMapName"), BootMapName, GGameIni);
	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("SocialMapName"), SocialMapName, GGameIni);
	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("PvpMapName"), PvpMapName, GGameIni);
	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("PracticeMapName"), PracticeMapName, GGameIni);

	if (BootMapName.IsEmpty()) BootMapName = TEXT("/Game/Maps/DL_Boot");
	if (SocialMapName.IsEmpty()) SocialMapName = TEXT("/Game/Maps/DL_Social");
	if (PvpMapName.IsEmpty()) PvpMapName = TEXT("/Game/Maps/DL_PvpArena");
	if (PracticeMapName.IsEmpty()) PracticeMapName = TEXT("/Game/Maps/DL_Practice");

	FString RaidCsv;
	GConfig->GetString(TEXT("/Script/DestinyLike.DLSceneSettings"), TEXT("RaidChamberMapNames"), RaidCsv, GGameIni);
	RaidChamberMapNames.Reset();
	RaidCsv.ParseIntoArray(RaidChamberMapNames, TEXT(","), true);
	if (RaidChamberMapNames.Num() == 0)
	{
		RaidChamberMapNames = {
			TEXT("/Game/Maps/DL_Raid_01"),
			TEXT("/Game/Maps/DL_Raid_02"),
			TEXT("/Game/Maps/DL_Raid_03"),
			TEXT("/Game/Maps/DL_Raid_04")
		};
	}
}

FString UDLSceneRouter::GetMapNameForScene(EDLSceneId Scene, int32 RaidChamberIndex) const
{
	switch (Scene)
	{
	case EDLSceneId::Boot: return BootMapName;
	case EDLSceneId::Social: return LastSocialMapName.IsEmpty() ? SocialMapName : LastSocialMapName;
	case EDLSceneId::Composer: return SocialMapName;
	case EDLSceneId::Pvp: return PvpMapName;
	case EDLSceneId::Practice: return PracticeMapName;
	case EDLSceneId::Raid:
		if (RaidChamberMapNames.IsValidIndex(RaidChamberIndex))
		{
			return RaidChamberMapNames[RaidChamberIndex];
		}
		return RaidChamberMapNames.Num() > 0 ? RaidChamberMapNames[0] : TEXT("/Game/Maps/DL_Raid_01");
	default: return SocialMapName;
	}
}

void UDLSceneRouter::RememberSocialMap(const FString& MapName)
{
	if (!MapName.IsEmpty())
	{
		LastSocialMapName = MapName;
		if (UDLGameInstance* GI = Cast<UDLGameInstance>(GetGameInstance()))
		{
			GI->SetLastSocialMap(MapName);
		}
	}
}

FString UDLSceneRouter::GameModePathForScene(EDLSceneId Scene)
{
	switch (Scene)
	{
	case EDLSceneId::Boot: return TEXT("/Script/DestinyLike.DLBootGameMode");
	case EDLSceneId::Social: return TEXT("/Script/DestinyLike.DLSocialGameMode");
	case EDLSceneId::Composer: return TEXT("/Script/DestinyLike.DLComposerGameMode");
	case EDLSceneId::Pvp: return TEXT("/Script/DestinyLike.DLPvpGameMode");
	case EDLSceneId::Practice: return TEXT("/Script/DestinyLike.DLPracticeGameMode");
	case EDLSceneId::Raid: return TEXT("/Script/DestinyLike.DLRaidGameMode");
	default: return TEXT("/Script/DestinyLike.DLSocialGameMode");
	}
}

void UDLSceneRouter::SoftTravel(const FString& MapName, EDLSceneId Scene)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString Options = FString::Printf(
		TEXT("game=%s?dl=%d?NoSeamlessTravel"),
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

void UDLSceneRouter::TravelToScene(EDLSceneId Scene, int32 RaidChamberIndex)
{
	CurrentScene = Scene;
	if (Scene == EDLSceneId::Social)
	{
		RememberSocialMap(GetMapNameForScene(EDLSceneId::Social));
	}

	UWorld* World = GetWorld();
	const FString MapName = GetMapNameForScene(Scene, RaidChamberIndex);
	if (World && World->HasBegunPlay())
	{
		TWeakObjectPtr<UDLSceneRouter> WeakThis(this);
		World->GetTimerManager().SetTimer(
			DeferredTravelTimer,
			FTimerDelegate::CreateLambda([WeakThis, MapName, Scene]()
			{
				if (UDLSceneRouter* Router = WeakThis.Get())
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

void UDLSceneRouter::TravelDeferred(const FString& MapName, EDLSceneId Scene)
{
	SoftTravel(MapName, Scene);
}

void UDLSceneRouter::ExitActivityToSocial()
{
	CurrentScene = EDLSceneId::Social;
	SoftTravel(GetMapNameForScene(EDLSceneId::Social), EDLSceneId::Social);
}
