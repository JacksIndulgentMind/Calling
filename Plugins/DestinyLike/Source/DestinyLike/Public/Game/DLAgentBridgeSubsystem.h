#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpRouteHandle.h"
#include "HttpResultCallback.h"
#include "Dom/JsonObject.h"
#include "Input/DLAgentIntent.h"
#include "Game/DLAgentGotoDriver.h"
#include "Game/DLAgentSequenceRunner.h"
#include "DLAgentBridgeSubsystem.generated.h"

struct FHttpServerRequest;
class IHttpRouter;
class ADLPlayerCharacter;

/**
 * Localhost test harness. Cursor MCP (or curl) drives the same pawn input path
 * (`FDLAgentIntent`). Probe and Recast link numbers live in NavTune.json.
 * Shipping builds never listen.
 */
UCLASS()
class DESTINYLIKE_API UDLAgentBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TSharedRef<FJsonObject> BuildStateJson(const FGuid& SeatId = FGuid()) const;
	FString BuildStateJsonString(const FGuid& SeatId = FGuid()) const;

private:
	void StartListener();
	void StopListener();
	void BindTickClock();
	void UnbindTickClock();

	bool IsLoopback(const FHttpServerRequest& Request) const;
	ADLPlayerCharacter* FindLocalPawn() const;
	APlayerController* FindLocalController() const;
	UWorld* GetWorldSafe() const;

	bool HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleIntent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleSequence(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleGoto(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleRespawn(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	void ApplyIntentObject(const TSharedPtr<FJsonObject>& Root, bool bCancelQueue);
	bool ParseSteps(const TSharedPtr<FJsonObject>& Root, TArray<FDLAgentStep>& OutSteps, bool& bAfterCurrent) const;
	FDLAgentStep ParseStep(const TSharedPtr<FJsonObject>& Obj) const;
	bool QueueSteps(const TArray<FDLAgentStep>& Steps, bool bAfterCurrent, FString& OutError);
	void CancelSequenceAndGoto(bool bClearPawn);
	void TickAgent(float DeltaSeconds);
	bool StartGoto(const FVector& Dest, FString& OutError, bool bFromRepath = false);
	void ApplyLookCommand(ADLPlayerCharacter* Char, const FDLLookCommand& Look) const;
	void ApplyStepHolds(ADLPlayerCharacter* Char, const FDLAgentStep& Step);
	void ApplyStepPulses(ADLPlayerCharacter* Char, const FDLAgentStep& Step);
	void FillSceneMenu(TSharedRef<FJsonObject> Root) const;
	bool HandleDirector(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleHub(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	ADLPlayerCharacter* ResolvePawn() const;
	ADLPlayerCharacter* ResolvePawnForSeat(const FGuid& SeatId) const;

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle StateRoute;
	FHttpRouteHandle IntentRoute;
	FHttpRouteHandle SequenceRoute;
	FHttpRouteHandle GotoRoute;
	FHttpRouteHandle RespawnRoute;
	FHttpRouteHandle DirectorRoute;
	FHttpRouteHandle HubRoute;
	uint32 Port = 18765;
	bool bAllowAgentInput = false;
	FGuid AgentSeatId;

	FDelegateHandle FixedTickHandle;

	FDLAgentSequenceRunner SequenceRunner;
	FDLAgentGotoDriver GotoDriver;
};
