#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Game/CLLobbyTypes.h"
#include "CLSessionHub.generated.h"

class FSocket;

/**
 * Session WebSocket (default 18766; cmdline `-CallingSessionHubPort=`).
 */
UCLASS()
class CALLING_API UCLSessionHub : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }

	void StartHost();
	void StopHost();
	bool IsListening() const { return ListenSocket != nullptr; }
	int32 GetPort() const { return Port; }
	void PushSnapshots(ECLHubSnapshotReason Reason, const FGuid& OnlySeat = FGuid());

protected:
	void AcceptClients();
	void PumpClient(int32 Index);
	bool TryUpgrade(int32 Index);
	void HandleText(int32 Index, const FString& Text);
	void SendText(int32 Index, const FString& Text);
	void CloseClient(int32 Index);
	static bool ReadHttpHeader(const TArray<uint8>& Buffer, FString& OutKey);
	static bool ReadUpgrade(const TArray<uint8>& Buffer, FString& OutKey, FString& OutMode, FString& OutTarget, FString& OutQuery);
	static FString MakeAcceptKey(const FString& ClientKey);
	static bool DecodeFrame(TArray<uint8>& Buffer, FString& OutText);

	struct FClient
	{
		FSocket* Socket = nullptr;
		TArray<uint8> Buffer;
		bool bUpgraded = false;
		FGuid SeatId;
		FGuid AgentId;
		bool bProxy = false;
		FGuid TargetInstance;
	};

	FSocket* ListenSocket = nullptr;
	TArray<FClient> Clients;
	int32 Port = 18766;
	bool bWantListen = false;
};
