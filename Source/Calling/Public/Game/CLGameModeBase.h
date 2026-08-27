#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/CLTypes.h"
#include "Game/CLGreyboxFloors.h"
#include "TimerManager.h"
#include "CLGameModeBase.generated.h"

class UCLActivityStateComponent;
class UCLEncounterDirector;

UCLASS()
class CALLING_API ACLGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACLGameModeBase();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void RequestExitToSocial();

	/** Teleport existing pawn to start, or RestartPlayer if the pawn was destroyed. */
	UFUNCTION(BlueprintCallable, Category = "Calling")
	void RequestRespawn(AController* Player);

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void AwardDropFromTable(FName TableId, AController* ToController);

	UFUNCTION(BlueprintPure, Category = "Calling")
	ECLSceneId GetSceneId() const { return SceneId; }

	/** Gate Go: PvP/Raid start the match. Open scenes no-op. */
	virtual void HandleLobbyGo();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling")
	ECLSceneId SceneId = ECLSceneId::Social;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling")
	TSubclassOf<APawn> DefaultCombatPawnClass;

	void SyncSceneToGameState() const;
	UCLActivityStateComponent* GetActivityState() const;

	bool bRespawnInProgress = false;
};

UCLASS()
class CALLING_API ACLBootGameMode : public ACLGameModeBase
{
	GENERATED_BODY()
public:
	ACLBootGameMode();
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	void TryShowBootProfileUI(APlayerController* PC);
	void TravelToSocialDeferred();

	UPROPERTY()
	TObjectPtr<class UCLBootProfileWidget> BootProfileWidget;

	FTimerHandle AutoEnterSocialTimer;
};

UCLASS()
class CALLING_API ACLSocialGameMode : public ACLGameModeBase
{
	GENERATED_BODY()
public:
	ACLSocialGameMode();
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Social")
	void SetHostSocialPvpMode(ECLSocialPvpMode Mode);

protected:
	void EnsureSocialGreybox();
	void ShowSocialMarker();

	UPROPERTY()
	TObjectPtr<class UCLSocialMarkerWidget> SocialMarker;
};

UCLASS()
class CALLING_API ACLComposerGameMode : public ACLGameModeBase
{
	GENERATED_BODY()
public:
	ACLComposerGameMode();
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void HandleLobbyGo() override;

protected:
	void EnsureComposerGreybox();
	void ShowComposerMenu();

	UPROPERTY()
	TObjectPtr<class UCLComposerMenu> ComposerMenu;
};

UCLASS()
class CALLING_API ACLPvpGameMode : public ACLGameModeBase
{
	GENERATED_BODY()
public:
	ACLPvpGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void HandleLobbyGo() override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Pvp")
	void StartMatchFromLobby();

	UFUNCTION(BlueprintCallable, Category = "Calling|Pvp")
	void EndMatchAndAward();
};

UCLASS()
class CALLING_API ACLRaidGameMode : public ACLGameModeBase
{
	GENERATED_BODY()
public:
	ACLRaidGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void HandleLobbyGo() override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Raid")
	void BeginChamber(int32 ChamberIndex);

	UFUNCTION(BlueprintCallable, Category = "Calling|Raid")
	void CompleteChamber();

	UFUNCTION(BlueprintCallable, Category = "Calling|Raid")
	void AdvanceOrFinishRaid();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling|Raid")
	TObjectPtr<UCLEncounterDirector> EncounterDirector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling|Raid")
	int32 ChamberCount = 4;

	int32 InferChamberIndexFromMap() const;
	static ECLGreyboxLayout GreyboxLayoutForChamber(int32 ChamberIndex);
};

UCLASS()
class CALLING_API ACLPracticeGameMode : public ACLGameModeBase
{
	GENERATED_BODY()
public:
	ACLPracticeGameMode();
	virtual void StartPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Practice")
	void SpawnPracticeDummies(int32 Count = 4);
};
