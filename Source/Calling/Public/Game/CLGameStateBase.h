#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/CLTypes.h"
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
	TArray<FCLSeatScore> SeatScores;

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
