#pragma once

#include "CoreMinimal.h"

enum class ECLBotOutcome : uint8
{
	None,
	Success,
	GoodEnough,
	Fail,
	Skipped
};

enum class ECLBotStmtKind : uint8
{
	Leaf,
	Ref,
	If,
	Stop
};

/** Why remaining walk was replaced. Hub `branchBotBook.cause` is required. */
enum class ECLBotBookBranchCause : uint8
{
	Invalid,
	/** Bot failed the book independent of outside factors. Reports `botbook_execution`. */
	Execution,
	/** Combat, personality, or other world change. Not an execution defect. */
	Situation
};

struct FCLBotPredicate
{
	FString Name;
	FString Op;
	FString Value;
	TArray<FString> OrValues;

	bool IsEmpty() const { return Name.IsEmpty(); }
};

struct FCLBotLeaf
{
	FName Verb = NAME_None;
	TMap<FString, FString> Params;
	TArray<FName> WhileVerbs;
	FCLBotPredicate Success;
	FCLBotPredicate GoodEnough;
	float TrySuccessFor = -1.f;
	float FailTimeout = 8.f;
	int32 Pulses = 1;
	float PulseGap = 0.12f;
	FString Move = TEXT("forward");
};

struct FCLBotStmt
{
	FString Id;
	ECLBotStmtKind Kind = ECLBotStmtKind::Stop;
	FCLBotLeaf Leaf;
	FName RefName = NAME_None;
	FCLBotPredicate IfPred;
	FString ElseTag;
	TArray<FCLBotStmt> ThenBody;
	TArray<FCLBotStmt> ElseBody;
};

struct FCLBotBook
{
	FName Name = NAME_None;
	TArray<FCLBotStmt> Body;
	TArray<FName> Fallbacks;
	FName OnRespawn = NAME_None;
	float DefaultTrySuccessFor = 2.5f;
	bool bAllowXyzGoto = false;
	bool bJit = false;
};

inline const TCHAR* CLBotOutcomeName(ECLBotOutcome Outcome)
{
	switch (Outcome)
	{
	case ECLBotOutcome::Success: return TEXT("success");
	case ECLBotOutcome::GoodEnough: return TEXT("goodEnough");
	case ECLBotOutcome::Fail: return TEXT("fail");
	case ECLBotOutcome::Skipped: return TEXT("skipped");
	default: return TEXT("none");
	}
}
