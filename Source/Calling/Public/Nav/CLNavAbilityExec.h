#pragma once

#include "CoreMinimal.h"
#include "Nav/CLNavAbilityEnvelope.h"

class ACLPlayerCharacter;
class UCLCombatMovementComponent;
struct FCLMovementTune;

enum class ECLNavAbilityExecMode : uint8
{
	JumpTo,
	AirDiveTo,
	Launch,
	SlideTo,
	DashTo,
	DodgeTo
};

/** Per-tick jump-to / airDive-to / slide-to / dash-to shared by BotBook leaves and goto. */
struct CALLING_API FCLNavAbilityExec
{
	ECLNavAbilityExecMode Mode = ECLNavAbilityExecMode::Launch;
	FVector Goal = FVector::ZeroVector;
	float LandRadius = 180.f;
	int32 JumpPulses = 5;
	float PulseGap = 0.12f;
	int32 Fired = 0;
	float Acc = 0.f;
	float Elapsed = 0.f;
	bool bDiveSeen = false;
	bool bDivePhase = false;
	bool bFinished = false;
	bool bFailed = false;
	int32 OffPadRetries = 0;
	bool bStickHeld = false;
	bool bPinUntilLand = false;
	FCLLaunchRecipe Recipe;
	FString TraceSub;
	bool bSlideLatched = false;
	bool bBurstFired = false;
	bool bBurstSeen = false;
	bool bLoggedStart = false;

	void Reset();
	void Start(ACLPlayerCharacter* Char);
	void Tick(float DeltaSeconds, ACLPlayerCharacter* Char);
	bool SuccessImpossible(ACLPlayerCharacter* Char) const;

	const TCHAR* ModeLabel() const;

private:
	float SampleAcc = 0.f;
	FString PhaseName;
	float VelXYMin = 0.f;
	float VelXYMax = 0.f;
	float VelXYSum = 0.f;
	float VelZMin = 0.f;
	float VelZMax = 0.f;
	int32 VelN = 0;

	void SetPhase(const TCHAR* Name, ACLPlayerCharacter* Char, const TCHAR* Extra = TEXT(""));
	void AccelVel(const FVector& Vel);
	void FlushVel();
	void LogStartIfNeeded(ACLPlayerCharacter* Char, const FCLNavAbilityBox& Box, const TCHAR* Sub, int32 Jumps);
	void LogMiss(ACLPlayerCharacter* Char, const FCLNavAbilityBox& Box, const TCHAR* Result);
	void TickJumpLaunch(float DeltaSeconds, ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move);
	void TickSlide(float DeltaSeconds, ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move);
	void TickBurst(float DeltaSeconds, ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move);
	FCLNavAbilityBox BoxFor(ACLPlayerCharacter* Char, UCLCombatMovementComponent* Move) const;
	void FaceGoal(ACLPlayerCharacter* Char, const FVector& Loc) const;
	bool FacingGoal(ACLPlayerCharacter* Char, const FVector& Loc) const;
	void ApplyLookupRecipe(UCLCombatMovementComponent* Move);
	void ApplyRecastOwnedFallback(UCLCombatMovementComponent* Move, const FCLMovementTune& Tune, ACLPlayerCharacter* Char);
};
