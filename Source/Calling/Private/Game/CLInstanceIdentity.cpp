#include "Game/CLInstanceIdentity.h"
#include "Game/CLAgentCodec.h"
#include "Game/CLAgentGotoDriver.h"
#include "Game/CLLoopbackJoin.h"
#include "Game/CLParticipantSeat.h"
#include "Core/CLLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"

using namespace CLAgentCodec;

namespace
{
	FGuid ParseAgentField(const TSharedPtr<FJsonObject>& Body)
	{
		if (!Body.IsValid())
		{
			return FGuid();
		}
		FGuid Id = ParseGuid(JsonStr(Body, TEXT("agentId")));
		if (!Id.IsValid())
		{
			Id = ParseGuid(JsonStr(Body, TEXT("agent")));
		}
		return Id;
	}
}

void UCLInstanceIdentitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InstanceId = FGuid::NewGuid();
	DeviceRequestorId = FGuid::NewGuid();
	UE_LOG(LogCalling, Display,
		TEXT("Calling instance %s device=%s pid=%d http=%d hub=%d"),
		*GuidStr(InstanceId),
		*GuidStr(DeviceRequestorId),
		FPlatformProcess::GetCurrentProcessId(),
		CLLoopbackJoin::AgentHttpPort(),
		CLLoopbackJoin::SessionHubPort());
}

UCLInstanceIdentitySubsystem* UCLInstanceIdentitySubsystem::Get(const UObject* WorldContext)
{
	const UGameInstance* GI = nullptr;
	if (const UGameInstance* AsGI = Cast<UGameInstance>(WorldContext))
	{
		GI = AsGI;
	}
	else if (WorldContext)
	{
		if (const UWorld* World = WorldContext->GetWorld())
		{
			GI = World->GetGameInstance();
		}
		else
		{
			GI = WorldContext->GetTypedOuter<UGameInstance>();
		}
	}
	return GI ? GI->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr;
}

void UCLInstanceIdentitySubsystem::CaptureOrigin(const TSharedPtr<FJsonObject>& Body)
{
	OriginInstanceId.Invalidate();
	if (!Body.IsValid())
	{
		return;
	}
	FGuid From = ParseGuid(JsonStr(Body, TEXT("originInstanceId")));
	if (!From.IsValid())
	{
		From = ParseGuid(JsonStr(Body, TEXT("instanceId")));
	}
	if (From.IsValid() && From != InstanceId)
	{
		OriginInstanceId = From;
	}
}

FGuid UCLInstanceIdentitySubsystem::Associate(const FGuid& AgentId)
{
	FGuid Id = AgentId;
	if (!Id.IsValid())
	{
		Id = FGuid::NewGuid();
	}
	const bool bNew = !Agents.Contains(Id);
	Agents.Add(Id, FDateTime::UtcNow());
	LastAgentId = Id;
	if (bNew)
	{
		UE_LOG(LogCallingHub, Display,
			TEXT("AgentConnect instance=%s agent=%s agents=%d"),
			*GuidStr(InstanceId), *GuidStr(Id), Agents.Num());
	}
	return Id;
}

FGuid UCLInstanceIdentitySubsystem::NoteJson(const TSharedPtr<FJsonObject>& Body, bool bMintIfMissing)
{
	CaptureOrigin(Body);
	FGuid Parsed = ParseAgentField(Body);
	if (!Parsed.IsValid() && !bMintIfMissing)
	{
		return FGuid();
	}
	const FGuid Id = Associate(Parsed);
	if (Body.IsValid())
	{
		Body->SetStringField(TEXT("agentId"), GuidStr(Id));
		if (OriginInstanceId.IsValid())
		{
			Body->SetStringField(TEXT("originInstanceId"), GuidStr(OriginInstanceId));
		}
	}
	return Id;
}

FGuid UCLInstanceIdentitySubsystem::NoteRequest(const FString& InHeaderAgentId, const FString& QueryAgentId, const TSharedPtr<FJsonObject>& Body, bool bMintIfMissing)
{
	FGuid Id = ParseGuid(InHeaderAgentId);
	if (!Id.IsValid())
	{
		Id = ParseGuid(QueryAgentId);
	}
	if (!Id.IsValid())
	{
		Id = ParseAgentField(Body);
	}
	CaptureOrigin(Body);
	if (!Id.IsValid() && !bMintIfMissing)
	{
		return FGuid();
	}
	Id = Associate(Id);
	if (Body.IsValid())
	{
		Body->SetStringField(TEXT("agentId"), GuidStr(Id));
		if (OriginInstanceId.IsValid())
		{
			Body->SetStringField(TEXT("originInstanceId"), GuidStr(OriginInstanceId));
		}
	}
	return Id;
}

bool UCLInstanceIdentitySubsystem::CheckInstance(const TSharedPtr<FJsonObject>& Body, FString& OutError) const
{
	const FGuid Caller = ParseGuid(JsonStr(Body, TEXT("instanceId")));
	if (Caller.IsValid() && Caller != InstanceId)
	{
		OutError = TEXT("instance_mismatch");
		UE_LOG(LogCallingHub, Error,
			TEXT("instance_mismatch caller=%s this=%s"),
			*GuidStr(Caller), *GuidStr(InstanceId));
		return false;
	}
	return true;
}

void UCLInstanceIdentitySubsystem::BindSeat(UCLParticipantSeat* Seat) const
{
	if (!Seat)
	{
		return;
	}
	if (!Seat->GetOwnerInstanceId().IsValid())
	{
		Seat->SetOwnerInstanceId(InstanceId);
	}
	if (LastAgentId.IsValid())
	{
		Seat->SetRequestingAgentId(LastAgentId);
		Seat->SetRequestorId(LastAgentId);
	}
}

void UCLInstanceIdentitySubsystem::StampJson(const TSharedRef<FJsonObject>& Out) const
{
	Out->SetStringField(TEXT("instanceId"), GuidStr(InstanceId));
	if (LastAgentId.IsValid())
	{
		Out->SetStringField(TEXT("agentId"), GuidStr(LastAgentId));
	}
	if (DeviceRequestorId.IsValid())
	{
		Out->SetStringField(TEXT("deviceRequestorId"), GuidStr(DeviceRequestorId));
	}
	if (OriginInstanceId.IsValid())
	{
		Out->SetStringField(TEXT("originInstanceId"), GuidStr(OriginInstanceId));
	}
}

void UCLInstanceIdentitySubsystem::StampGoto(FCLAgentGotoDriver& Goto, const UCLParticipantSeat* Seat) const
{
	Goto.RequestInstanceId = InstanceId;
	Goto.RequestAgentId = Seat ? Seat->GetRequestingAgentId() : LastAgentId;
	Goto.RequestorId = Seat && Seat->GetRequestorId().IsValid()
		? Seat->GetRequestorId()
		: (LastAgentId.IsValid() ? LastAgentId : DeviceRequestorId);
}
