#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/DLTypes.h"
#include "Game/DLGreyboxFloors.h"
#include "TimerManager.h"
#include "DLGameModeBase.generated.h"

class UDLActivityStateComponent;
class UDLEncounterDirector;

UCLASS()
class DESTINYLIKE_API ADLGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADLGameModeBase();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void RequestExitToSocial();

	/** Teleport existing pawn to start, or RestartPlayer if the pawn was destroyed. */
	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void RequestRespawn(AController* Player);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void AwardDropFromTable(FName TableId, AController* ToController);

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	EDLSceneId GetSceneId() const { return SceneId; }

	/** Gate Go: PvP/Raid start the match. Open scenes no-op. */
	virtual void HandleLobbyGo();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike")
	EDLSceneId SceneId = EDLSceneId::Social;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike")
	TSubclassOf<APawn> DefaultCombatPawnClass;

	void SyncSceneToGameState() const;
	UDLActivityStateComponent* GetActivityState() const;

	bool bRespawnInProgress = false;
};

UCLASS()
class DESTINYLIKE_API ADLBootGameMode : public ADLGameModeBase
{
	GENERATED_BODY()
public:
	ADLBootGameMode();
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	void TryShowBootProfileUI(APlayerController* PC);
	void TravelToSocialDeferred();

	UPROPERTY()
	TObjectPtr<class UDLBootProfileWidget> BootProfileWidget;

	FTimerHandle AutoEnterSocialTimer;
};

UCLASS()
class DESTINYLIKE_API ADLSocialGameMode : public ADLGameModeBase
{
	GENERATED_BODY()
public:
	ADLSocialGameMode();
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Social")
	void SetHostSocialPvpMode(EDLSocialPvpMode Mode);

protected:
	void EnsureSocialGreybox();
	void ShowSocialMarker();

	UPROPERTY()
	TObjectPtr<class UDLSocialMarkerWidget> SocialMarker;
};

UCLASS()
class DESTINYLIKE_API ADLComposerGameMode : public ADLGameModeBase
{
	GENERATED_BODY()
public:
	ADLComposerGameMode();
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void HandleLobbyGo() override;

protected:
	void EnsureComposerGreybox();
	void ShowComposerMenu();

	UPROPERTY()
	TObjectPtr<class UDLComposerMenu> ComposerMenu;
};

UCLASS()
class DESTINYLIKE_API ADLPvpGameMode : public ADLGameModeBase
{
	GENERATED_BODY()
public:
	ADLPvpGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void HandleLobbyGo() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Pvp")
	void StartMatchFromLobby();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Pvp")
	void EndMatchAndAward();
};

UCLASS()
class DESTINYLIKE_API ADLRaidGameMode : public ADLGameModeBase
{
	GENERATED_BODY()
public:
	ADLRaidGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void HandleLobbyGo() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Raid")
	void BeginChamber(int32 ChamberIndex);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Raid")
	void CompleteChamber();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Raid")
	void AdvanceOrFinishRaid();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike|Raid")
	TObjectPtr<UDLEncounterDirector> EncounterDirector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike|Raid")
	int32 ChamberCount = 4;

	int32 InferChamberIndexFromMap() const;
	static EDLGreyboxLayout GreyboxLayoutForChamber(int32 ChamberIndex);
};

UCLASS()
class DESTINYLIKE_API ADLPracticeGameMode : public ADLGameModeBase
{
	GENERATED_BODY()
public:
	ADLPracticeGameMode();
	virtual void StartPlay() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Practice")
	void SpawnPracticeDummies(int32 Count = 4);
};
