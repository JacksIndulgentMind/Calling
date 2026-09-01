#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/CLTypes.h"
#include "Game/CLLobbyTypes.h"
#include "CLDirectorPanel.generated.h"

class UVerticalBox;

/** Director travel, lobby ready/team, and social host. */
UCLASS()
class CALLING_API UCLDirectorPanel : public UObject
{
	GENERATED_BODY()

public:
	void JumpToActivity(ECLSceneId Scene, int32 RaidChamberIndex = 0);
	void ExitToSocial();
	void UnsetDefaultProfile();
	void HostSocialLobby(ECLSocialPvpMode Mode);
	void HostSocialAudience(ECLSocialDefaultKind Kind);
	void JoinSocialHost(const FString& Host, int32 Port);
	void SaveSocialDefault(ECLSocialDefaultKind Kind, const FString& Host, int32 Port, ECLSocialJoinFallback Fallback);
	void RefreshActivityLobbies(ECLSceneId Activity);
	void JoinListedLobby(int32 Index);
	void StartLoopbackHost();
	void JoinLoopback(const FString& Selected = TEXT(""));
	void ToggleLocalReady();
	void JoinTeam(ECLPvpTeam Team);
	void RequestGo();
	void HostSocialClosed();
};
