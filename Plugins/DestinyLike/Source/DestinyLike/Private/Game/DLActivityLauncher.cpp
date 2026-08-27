#include "Game/DLActivityLauncher.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLLobbyTypes.h"
#include "Game/DLGameModeBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

void DLActivityLauncher::Travel(UObject* WorldContext, EDLSceneId Scene, int32 RaidChamberIndex)
{
	if (Scene == EDLSceneId::Boot || !WorldContext)
	{
		return;
	}
	UWorld* World = WorldContext->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UDLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		if (Scene == EDLSceneId::Composer)
		{
			int32 MaxPlayers = 8;
			GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
			Lobby->SetPendingInvoice(FDLLobbyInvoice::MakeComposerPvp(2, MaxPlayers));
		}
		else if (Scene == EDLSceneId::Pvp)
		{
			int32 MaxPlayers = 8;
			GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
			Lobby->SetPendingInvoice(FDLLobbyInvoice::MakePvp(1, MaxPlayers));
		}
		else if (Scene == EDLSceneId::Raid)
		{
			int32 MaxPlayers = 6;
			GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Raid"), MaxPlayers, GGameIni);
			Lobby->SetPendingInvoice(FDLLobbyInvoice::MakeRaid(RaidChamberIndex, 1, MaxPlayers));
		}
		else
		{
			Lobby->ClearPendingInvoice();
		}
	}
	if (UDLSceneRouter* Router = GI ? GI->GetSubsystem<UDLSceneRouter>() : nullptr)
	{
		Router->TravelToScene(Scene, RaidChamberIndex);
	}
}

void DLActivityLauncher::ExitToSocial(UObject* WorldContext)
{
	if (!WorldContext)
	{
		return;
	}
	if (ADLGameModeBase* GM = Cast<ADLGameModeBase>(UGameplayStatics::GetGameMode(WorldContext)))
	{
		GM->RequestExitToSocial();
		return;
	}
	UWorld* World = WorldContext->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UDLSceneRouter* Router = GI ? GI->GetSubsystem<UDLSceneRouter>() : nullptr)
	{
		Router->ExitActivityToSocial();
	}
}
