#pragma once

#include "CoreMinimal.h"
#include "AI/CLBotBookTypes.h"

class UCLParticipantSeat;
class ACLPlayerCharacter;
class UCLSeatMotor;
class UWorld;

struct FCLBotVerbContext
{
	UWorld* World = nullptr;
	UCLParticipantSeat* Seat = nullptr;
	ACLPlayerCharacter* Char = nullptr;
	UCLSeatMotor* Motor = nullptr;
	const FCLBotLeaf* Leaf = nullptr;
	FVector Goal = FVector::ZeroVector;
	FName MarkerId = NAME_None;
	FGuid FocusSeat;
	float Elapsed = 0.f;
};

struct ICLBotVerb
{
	virtual ~ICLBotVerb() = default;
	virtual void Start(FCLBotVerbContext& Ctx) = 0;
	virtual void Tick(float DeltaSeconds, FCLBotVerbContext& Ctx) = 0;
	virtual void Stop(FCLBotVerbContext& Ctx) = 0;
	virtual bool SuccessImpossible(const FCLBotVerbContext& Ctx) const { (void)Ctx; return false; }
};

TUniquePtr<ICLBotVerb> CLMakeBotVerb(FName VerbId);
void CLApplyBotWhile(FCLBotVerbContext& Ctx, bool bIncludeMove);
