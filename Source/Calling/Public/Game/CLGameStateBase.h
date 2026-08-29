#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/CLTypes.h"
#include "Game/CLLobbyTypes.h"
#include "CLGameStateBase.generated.h"

class UCLActivityStateComponent;
class AController;
class APawn;

USTRUCT(BlueprintType)
struct CALLING_API FCLSeatScore
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SeatId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	float Score = 0.f;

	UPROPERTY()
	int32 FinalBlows = 0;

	UPROPERTY()
	int32 Assists = 0;
};

UCLASS()
class CALLING_API ACLGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	ACLGameStateBase();

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLActivityStateComponent* GetActivityState() const { return ActivityState; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	ECLSceneId GetSceneId() const { return SceneId; }

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void SetSceneId(ECLSceneId InSceneId);

	UFUNCTION(BlueprintPure, Category = "Calling")
	ECLSocialPvpMode GetSocialPvpMode() const { return SocialPvpMode; }

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void SetSocialPvpMode(ECLSocialPvpMode Mode);

	UFUNCTION(BlueprintPure, Category = "Calling")
	int32 GetRaidChamberIndex() const { return RaidChamberIndex; }

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void SetRaidChamberIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Calling")
	bool CanPlayersDamageEachOther() const;

	void RegisterTakeOut(AController* Killer, APawn* Victim, const TArray<AController*>& Assists);
	FString GetScoreLine() const;
	float GetTeamAScore() const { return TeamAScore; }
	float GetTeamBScore() const { return TeamBScore; }
	UFUNCTION(BlueprintPure, Category = "Calling|Score")
	int32 GetTeamAKills() const { return TeamAKills; }

	UFUNCTION(BlueprintPure, Category = "Calling|Score")
	int32 GetTeamBKills() const { return TeamBKills; }

	void AddTeamFinalBlow(ECLPvpTeam Team);

	UFUNCTION(BlueprintPure, Category = "Calling|Mode")
	FName GetLiveShrine() const { return LiveShrine; }

	UFUNCTION(BlueprintPure, Category = "Calling|Mode")
	FString GetModeResult() const { return ModeResult; }

	UFUNCTION(BlueprintPure, Category = "Calling|Mode")
	FString GetWinningTeam() const { return WinningTeam; }

	UFUNCTION(BlueprintPure, Category = "Calling|Mode")
	FString GetModeFailReason() const { return ModeFailReason; }

	UFUNCTION(BlueprintPure, Category = "Calling|Mode")
	bool GetShrineHeldRed() const { return bShrineHeldRed; }

	UFUNCTION(BlueprintPure, Category = "Calling|Mode")
	bool GetShrineHeldBlue() const { return bShrineHeldBlue; }

	void SetLiveShrine(FName Id);
	void SetShrineHeld(ECLPvpTeam Team, bool bHeld);
	void SetModeOutcome(const FString& Result, const FString& Winner, const FString& FailReason);

	void SetLobbySnapshot(const TArray<FCLLobbySeatSnap>& Seats, int32 Ready, int32 MinPlayers, bool bQueued);
	const TArray<FCLLobbySeatSnap>& GetLobbySeats() const { return LobbySeats; }
	int32 GetLobbyReady() const { return LobbyReady; }
	int32 GetLobbyMinPlayers() const { return LobbyMinPlayers; }
	bool IsLobbyStartQueued() const { return bLobbyStartQueued; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLActivityStateComponent> ActivityState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling")
	ECLSceneId SceneId = ECLSceneId::Social;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling")
	ECLSocialPvpMode SocialPvpMode = ECLSocialPvpMode::Optional;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling")
	int32 RaidChamberIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Score")
	float TeamAScore = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Score")
	float TeamBScore = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Score")
	int32 TeamAKills = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Score")
	int32 TeamBKills = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Mode")
	FName LiveShrine;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Mode")
	bool bShrineHeldRed = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Mode")
	bool bShrineHeldBlue = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Mode")
	FString ModeResult = TEXT("in_progress");

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Mode")
	FString WinningTeam;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Mode")
	FString ModeFailReason;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Score")
	TArray<FCLSeatScore> SeatScores;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Lobby")
	TArray<FCLLobbySeatSnap> LobbySeats;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Lobby")
	int32 LobbyReady = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Lobby")
	int32 LobbyMinPlayers = 2;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Lobby")
	bool bLobbyStartQueued = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class CALLING_API ACLBootGameState : public ACLGameStateBase
{
	GENERATED_BODY()
public:
	ACLBootGameState();
};

UCLASS()
class CALLING_API ACLSocialGameState : public ACLGameStateBase
{
	GENERATED_BODY()
public:
	ACLSocialGameState();
};

UCLASS()
class CALLING_API ACLComposerGameState : public ACLGameStateBase
{
	GENERATED_BODY()
public:
	ACLComposerGameState();
};

UCLASS()
class CALLING_API ACLPvpGameState : public ACLGameStateBase
{
	GENERATED_BODY()
public:
	ACLPvpGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class CALLING_API ACLRaidGameState : public ACLGameStateBase
{
	GENERATED_BODY()
public:
	ACLRaidGameState();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Raid")
	int32 ChambersCompleted = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Raid")
	bool bChamberCleared = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class CALLING_API ACLPracticeGameState : public ACLGameStateBase
{
	GENERATED_BODY()
public:
	ACLPracticeGameState();
};
