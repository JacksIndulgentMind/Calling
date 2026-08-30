#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "CLInstanceIdentity.generated.h"

class UCLParticipantSeat;
struct FCLAgentGotoDriver;

/**
 * One UUID per Unreal process (host vs two-box guest are different).
 * Connecting agents send `agentId`; this subsystem associates that id with this
 * instance and stamps both on every agent-requested action.
 */
UCLASS()
class CALLING_API UCLInstanceIdentitySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	FGuid GetInstanceId() const { return InstanceId; }
	FGuid GetLastAgentId() const { return LastAgentId; }
	FGuid GetOriginInstanceId() const { return OriginInstanceId; }
	FGuid GetDeviceRequestorId() const { return DeviceRequestorId; }

	/** Header, query, then JSON `agentId`. POST mints if missing; GET should pass bMintIfMissing=false.
	 * Does not overwrite caller `instanceId`. Omitted instanceId means this process. */
	FGuid NoteRequest(const FString& HeaderAgentId, const FString& QueryAgentId, const TSharedPtr<FJsonObject>& Body, bool bMintIfMissing = true);
	FGuid NoteJson(const TSharedPtr<FJsonObject>& Body, bool bMintIfMissing = true);
	bool CheckInstance(const TSharedPtr<FJsonObject>& Body, FString& OutError) const;

	void BindSeat(UCLParticipantSeat* Seat) const;
	void StampJson(const TSharedRef<FJsonObject>& Out) const;
	void StampGoto(FCLAgentGotoDriver& Goto, const UCLParticipantSeat* Seat) const;

	static UCLInstanceIdentitySubsystem* Get(const UObject* WorldContext);

protected:
	FGuid Associate(const FGuid& AgentId);
	void CaptureOrigin(const TSharedPtr<FJsonObject>& Body);

	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	FGuid LastAgentId;

	UPROPERTY()
	FGuid OriginInstanceId;

	/** Per local input device (mouse+keyboard pair) for this process lifetime. */
	UPROPERTY()
	FGuid DeviceRequestorId;

	TMap<FGuid, FDateTime> Agents;
};
