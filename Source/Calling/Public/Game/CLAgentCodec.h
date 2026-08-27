#pragma once

#include "CoreMinimal.h"
#include "Game/CLAgentSequenceRunner.h"
#include "Input/CLAgentIntent.h"
#include "Dom/JsonObject.h"

/** Shared JSON helpers for hub / director / loopback codecs. */
namespace CLAgentCodec
{
	bool JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool bDefault = false);
	float JsonNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float Default = 0.f);
	FString JsonStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default = FString());
	TSharedPtr<FJsonObject> JsonObj(const TSharedPtr<FJsonObject>& Root, const TCHAR* Key);
	FString JsonToString(const TSharedRef<FJsonObject>& Root);
	FCLLookCommand ParseLook(const TSharedPtr<FJsonObject>& LookObj);
	FCLAgentStep ParseStep(const TSharedPtr<FJsonObject>& Obj);
	bool ParseSteps(const TSharedPtr<FJsonObject>& Root, TArray<FCLAgentStep>& OutSteps, bool& bRemainder);
	FGuid ParseGuid(const FString& Text);
	FString GuidStr(const FGuid& Id);
	FCLAgentIntent IntentFromObject(const TSharedPtr<FJsonObject>& Root, FGuid& OutTrackSeatId);
}
