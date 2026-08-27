#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/DLTypes.h"
#include "DLGameStateBase.generated.h"

class UDLActivityStateComponent;
class AController;
class APawn;

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLSeatScore
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
class DESTINYLIKE_API ADLGameStateBase : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADLGameStateBase();

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLActivityStateComponent* GetActivityState() const { return ActivityState; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	EDLSceneId GetSceneId() const { return SceneId; }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void SetSceneId(EDLSceneId InSceneId);

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	EDLSocialPvpMode GetSocialPvpMode() const { return SocialPvpMode; }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void SetSocialPvpMode(EDLSocialPvpMode Mode);

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	int32 GetRaidChamberIndex() const { return RaidChamberIndex; }

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void SetRaidChamberIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	bool CanPlayersDamageEachOther() const;

	void RegisterTakeOut(AController* Killer, APawn* Victim, const TArray<AController*>& Assists);
	FString GetScoreLine() const;
	float GetTeamAScore() const { return TeamAScore; }
	float GetTeamBScore() const { return TeamBScore; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLActivityStateComponent> ActivityState;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike")
	EDLSceneId SceneId = EDLSceneId::Social;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike")
	EDLSocialPvpMode SocialPvpMode = EDLSocialPvpMode::Optional;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike")
	int32 RaidChamberIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Score")
	float TeamAScore = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Score")
	float TeamBScore = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Score")
	TArray<FDLSeatScore> SeatScores;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class DESTINYLIKE_API ADLBootGameState : public ADLGameStateBase
{
	GENERATED_BODY()
public:
	ADLBootGameState();
};

UCLASS()
class DESTINYLIKE_API ADLSocialGameState : public ADLGameStateBase
{
	GENERATED_BODY()
public:
	ADLSocialGameState();
};

UCLASS()
class DESTINYLIKE_API ADLComposerGameState : public ADLGameStateBase
{
	GENERATED_BODY()
public:
	ADLComposerGameState();
};

UCLASS()
class DESTINYLIKE_API ADLPvpGameState : public ADLGameStateBase
{
	GENERATED_BODY()
public:
	ADLPvpGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class DESTINYLIKE_API ADLRaidGameState : public ADLGameStateBase
{
	GENERATED_BODY()
public:
	ADLRaidGameState();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Raid")
	int32 ChambersCompleted = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Raid")
	bool bChamberCleared = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class DESTINYLIKE_API ADLPracticeGameState : public ADLGameStateBase
{
	GENERATED_BODY()
public:
	ADLPracticeGameState();
};
