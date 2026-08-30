#include "Game/CLHubIngress.h"
#include "Game/CLAgentCodec.h"

using namespace CLAgentCodec;

namespace
{
	bool IsProxyToken(const FString& Token)
	{
		const FString T = Token.TrimStartAndEnd().ToLower();
		return T == TEXT("proxy") || T == TEXT("via");
	}

	FGuid FirstGuid(const FString& A, const FString& B, const FString& C)
	{
		FGuid Id = ParseGuid(A);
		if (!Id.IsValid())
		{
			Id = ParseGuid(B);
		}
		if (!Id.IsValid())
		{
			Id = ParseGuid(C);
		}
		return Id;
	}
}

FCLHubConnect CLHubIngress::Parse(
	const TSharedPtr<FJsonObject>& Root,
	const FString& HeaderMode,
	const FString& HeaderTarget,
	const FString& QueryMode,
	const FString& QueryTarget)
{
	FCLHubConnect Out;
	FString Mode = HeaderMode;
	if (Mode.IsEmpty())
	{
		Mode = QueryMode;
	}
	if (Mode.IsEmpty())
	{
		Mode = JsonStr(Root, TEXT("connectMode"));
	}
	Out.bProxy = IsProxyToken(Mode);

	Out.TargetInstance = FirstGuid(
		HeaderTarget,
		QueryTarget,
		JsonStr(Root, TEXT("targetInstanceId")));
	if (!Out.TargetInstance.IsValid())
	{
		Out.TargetInstance = ParseGuid(JsonStr(Root, TEXT("targetInstance")));
	}

	Out.ViaSeat = ParseGuid(JsonStr(Root, TEXT("via")));
	if (Out.ViaSeat.IsValid() && !Out.bProxy)
	{
		Out.bProxy = true;
	}
	return Out;
}

void CLHubIngress::StripProxyFields(const TSharedPtr<FJsonObject>& Root)
{
	if (!Root.IsValid())
	{
		return;
	}
	Root->RemoveField(TEXT("via"));
	Root->RemoveField(TEXT("connectMode"));
	Root->RemoveField(TEXT("targetInstanceId"));
	Root->RemoveField(TEXT("targetInstance"));
	Root->RemoveField(TEXT("instanceId"));
}
