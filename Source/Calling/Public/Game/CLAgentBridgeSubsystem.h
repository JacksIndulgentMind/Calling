#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpRouteHandle.h"
#include "HttpResultCallback.h"
#include "Dom/JsonObject.h"
#include "CLAgentBridgeSubsystem.generated.h"

struct FHttpServerRequest;
class IHttpRouter;
class ACLPlayerCharacter;
class UCLRemoteAgentSeatMotor;
class UCLLobbySubsystem;

/**
 * Loopback HTTP codec (18765). Hub JSON goes through FCLHubCommandRegistry.
 * Director through FCLDirectorCommandRegistry. State through FCLAgentStateSerializer.
 * Sequence/goto/intent are aliases onto the seat motor — no second clock.
 */
UCLASS()
class CALLING_API UCLAgentBridgeSubsystem : public UGameInstanceSubsystem
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

	bool IsLoopback(const FHttpServerRequest& Request) const;
	ACLPlayerCharacter* FindLocalPawn() const;
	APlayerController* FindLocalController() const;
	UWorld* GetWorldSafe() const;
	UCLLobbySubsystem* GetLobby() const;
	UCLRemoteAgentSeatMotor* ResolveMotor(FGuid& InOutSeatId) const;

	bool HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleIntent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleSequence(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleGoto(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleRespawn(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleDirector(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleHub(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	ACLPlayerCharacter* ResolvePawn() const;
	ACLPlayerCharacter* ResolvePawnForSeat(const FGuid& SeatId) const;

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
};
