#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Input/DLAgentIntent.h"

class ADLPlayerCharacter;

struct FDLAgentStep
{
	float Seconds = 0.f;
	FVector2D Move = FVector2D::ZeroVector;
	FDLLookCommand Look;
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

	FDLAgentIntent ToIntent(bool bPulses) const
	{
		FDLAgentIntent Intent;
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

struct DESTINYLIKE_API FDLAgentSequenceRunner
{
	TArray<FDLAgentStep> Steps;
	int32 Index = 0;
	float TimeLeft = 0.f;
	bool bPulsesSent = false;

	bool IsActive() const { return Steps.Num() > 0; }
	float RemainingSeconds() const;
	void Cancel();
	bool Queue(const TArray<FDLAgentStep>& NewSteps, bool bAfterCurrent, FString& OutError);
	void Tick(float DeltaSeconds, ADLPlayerCharacter* Char,
		TFunctionRef<void(ADLPlayerCharacter*, const FDLAgentStep&)> ApplyPulses,
		TFunctionRef<void(ADLPlayerCharacter*, const FDLAgentStep&)> ApplyHolds);
};
