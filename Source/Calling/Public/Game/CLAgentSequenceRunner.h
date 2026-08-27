#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Input/CLAgentIntent.h"

class ACLPlayerCharacter;

struct FCLAgentStep
{
	float Seconds = 0.f;
	FVector2D Move = FVector2D::ZeroVector;
	FCLLookCommand Look;
	bool bSprint = false;
	bool bCrouch = false;
	bool bADS = false;
	bool bFire = false;
	bool bJump = false;
	bool bDodge = false;
	bool bDash = false;
	bool bReload = false;
	bool bSwap = false;
	bool bSlide = false;
	bool bAirDive = false;
	bool bMelee = false;
	bool bWeaponPrimary = false;
	bool bWeaponSpecial = false;
	FName SightId = NAME_None;
	FGuid TrackSeatId;

	FCLAgentIntent ToIntent(bool bPulses) const
	{
		FCLAgentIntent Intent;
		Intent.Move = Move;
		Intent.bSprint = bSprint;
		Intent.bCrouch = bCrouch;
		Intent.bADS = bADS;
		Intent.bFire = bFire;
		if (bPulses)
		{
			Intent.Look = Look.GetDelta();
			Intent.bJump = bJump;
			Intent.bDodge = bDodge;
			Intent.bDash = bDash;
			Intent.bReload = bReload;
			Intent.bSwap = bSwap;
			Intent.bSlide = bSlide;
			Intent.bAirDive = bAirDive;
			Intent.bMelee = bMelee;
			Intent.bWeaponPrimary = bWeaponPrimary;
			Intent.bWeaponSpecial = bWeaponSpecial;
			Intent.SightId = SightId;
		}
		return Intent;
	}
};

struct CALLING_API FCLAgentSequenceRunner
{
	TArray<FCLAgentStep> Steps;
	int32 Index = 0;
	float TimeLeft = 0.f;
	bool bPulsesSent = false;

	bool IsActive() const { return Steps.Num() > 0; }
	float RemainingSeconds() const;
	void Cancel();
	bool Queue(const TArray<FCLAgentStep>& NewSteps, bool bAfterCurrent, FString& OutError);
	void Tick(float DeltaSeconds, ACLPlayerCharacter* Char,
		TFunctionRef<void(ACLPlayerCharacter*, const FCLAgentStep&)> ApplyPulses,
		TFunctionRef<void(ACLPlayerCharacter*, const FCLAgentStep&)> ApplyHolds);
};
