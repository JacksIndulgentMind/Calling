#include "Game/CLActivityLauncher.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLLobbyTypes.h"
#include "Game/CLGameModeBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

void CLActivityLauncher::Travel(UObject* WorldContext, ECLSceneId Scene, int32 RaidChamberIndex)
{
	if (Scene == ECLSceneId::Boot || !WorldContext)
	{
		return;
	}
	UWorld* World = WorldContext->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		if (Scene == ECLSceneId::Composer)
		{
			int32 MaxPlayers = 8;
			GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
			Lobby->SetPendingInvoice(FCLLobbyInvoice::MakeComposerPvp(2, MaxPlayers));
		}
		else if (Scene == ECLSceneId::Pvp)
		{
			int32 MaxPlayers = 8;
			GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
			Lobby->SetPendingInvoice(FCLLobbyInvoice::MakePvp(1, MaxPlayers));
		}
		else if (Scene == ECLSceneId::Raid)
		{
			int32 MaxPlayers = 6;
			GConfig->GetInt(TEXT("/Script/Calling.CLSessionSettings"), TEXT("MaxLobbyPlayers_Raid"), MaxPlayers, GGameIni);
			Lobby->SetPendingInvoice(FCLLobbyInvoice::MakeRaid(RaidChamberIndex, 1, MaxPlayers));
		}
		else
		{
			Lobby->ClearPendingInvoice();
		}
	}
	if (UCLSceneRouter* Router = GI ? GI->GetSubsystem<UCLSceneRouter>() : nullptr)
	{
		Router->TravelToScene(Scene, RaidChamberIndex);
	}
}

void CLActivityLauncher::ExitToSocial(UObject* WorldContext)
{
	if (!WorldContext)
	{
		return;
	}
	if (ACLGameModeBase* GM = Cast<ACLGameModeBase>(UGameplayStatics::GetGameMode(WorldContext)))
	{
		GM->RequestExitToSocial();
		return;
	}
	UWorld* World = WorldContext->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UCLSceneRouter* Router = GI ? GI->GetSubsystem<UCLSceneRouter>() : nullptr)
	{
		Router->ExitActivityToSocial();
	}
}
