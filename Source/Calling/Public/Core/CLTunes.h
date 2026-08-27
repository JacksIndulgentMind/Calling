#pragma once

#include "CoreMinimal.h"
#include "CLTunes.generated.h"

USTRUCT(BlueprintType)
struct CALLING_API FCLTickTune
{
	GENERATED_BODY()

	UPROPERTY() float GameSimHz = 30.f;
	UPROPERTY() float NetHz = 20.f;
	UPROPERTY() bool bLockGameSimToFixedStep = true;

	void LoadFromIni();
};

USTRUCT(BlueprintType)
struct CALLING_API FCLMovementTune
{
	GENERATED_BODY()

	UPROPERTY() float BaseWalkSpeed = 420.f;
	UPROPERTY() float BaseStrafeSpeed = 380.f;
	UPROPERTY() float SprintSpeedMultiplier = 1.70f;
	UPROPERTY() float ADSMovePenalty = 0.25f;
	UPROPERTY() float SlideDuration = 1.10f;
	UPROPERTY() float SlidePeakMultiplier = 1.45f;
	UPROPERTY() float SlideEndMultiplier = 0.85f;
	UPROPERTY() float SlideAccelPortion = 0.35f;
	UPROPERTY() bool bAllowADSWhileSliding = true;
	UPROPERTY() bool bAllowSlideDodgeCancel = true;
	UPROPERTY() bool bAllowSlideDashCancel = true;
	UPROPERTY() int32 MaxJumps = 3;
	UPROPERTY() float DashCooldownSeconds = 4.f;
	UPROPERTY() float DashDistance = 950.f;
	UPROPERTY() float DashDuration = 0.32f;
	UPROPERTY() float DashHopZ = 200.f;
	UPROPERTY() float DodgeDistance = 820.f;
	UPROPERTY() float DodgeForwardScale = 0.72f;
	UPROPERTY() float DodgeDuration = 0.58f;
	UPROPERTY() float DodgeIFrames = 0.2f;
	UPROPERTY() float DodgeCooldownSeconds = 4.f;
	UPROPERTY() float MantleReachDistance = 80.f;
	UPROPERTY() float CrouchTransitionSeconds = 0.32f;
	UPROPERTY() float CrouchHalfHeight = 48.f;
	UPROPERTY() float AirDiveDownSpeed = -4000.f;
	UPROPERTY() float AirDiveXYKeep = 0.90f;
	UPROPERTY() float AirDiveSteer = 1.75f;
	UPROPERTY() float AirDiveXYBrake = 3.5f;
	UPROPERTY() float AirDiveMaxXY = 1200.f;
	UPROPERTY() float AirDiveCooldownSeconds = 0.8f;
	UPROPERTY() float AirDiveCoalesceSeconds = 0.1f;
	UPROPERTY() float DiveHangSeconds = 0.44f;
	UPROPERTY() float DiveHangGravity = 0.15f;
	UPROPERTY() float DiveFallGravity = 8.0f;

	void LoadFromIni();
};

USTRUCT(BlueprintType)
struct CALLING_API FCLCombatTune
{
	GENERATED_BODY()

	UPROPERTY() float MaxShield = 100.f;
	UPROPERTY() float MaxHealth = 100.f;
	UPROPERTY() float ShieldRegenDelay = 3.f;
	UPROPERTY() float ShieldRegenPerSecond = 100.f;
	UPROPERTY() float HealthRegenDelay = 3.f;
	UPROPERTY() float HealthRegenPerSecond = 33.333f;
	UPROPERTY() float FlinchAimPunchDegrees = 8.f;
	UPROPERTY() float FlinchRecoveryPerSecond = 8.f;
	UPROPERTY() float RangeOptimalMinCm = 1600.f;
	UPROPERTY() float RangeOptimalMaxCm = 9000.f;
	UPROPERTY() float RangeFalloffMul = 2.1f;

	void LoadFromIni();
};

USTRUCT(BlueprintType)
struct CALLING_API FCLWeaponMotorTune
{
	GENERATED_BODY()

	UPROPERTY() float MinADSSeconds = 0.12f;
	UPROPERTY() float MaxADSSeconds = 0.32f;
	UPROPERTY() float MinReadySeconds = 0.18f;
	UPROPERTY() float MaxReadySeconds = 0.45f;
	UPROPERTY() float MinStowSeconds = 0.15f;
	UPROPERTY() float MaxStowSeconds = 0.40f;
	UPROPERTY() float RecoilAdsRecoverPerSecond = 14.f;
	UPROPERTY() float RecoilViewKickRecover = 18.f;
	UPROPERTY() float RecoilImpulseSeconds = 0.09f;
	UPROPERTY() float HipRecoilScale = 0.20f;
	UPROPERTY() float AdsRecoilScale = 0.16f;
	UPROPERTY() float RecoilRpmKnee = 700.f;
	UPROPERTY() float DefaultHipFOV = 90.f;
	UPROPERTY() float SlideHandlingBonus = 0.12f;
	UPROPERTY() float AimAssistConeDegrees = 3.2f;
	UPROPERTY() float AimAssistMagnetism = 0.55f;
	UPROPERTY() float ThirdPersonArmLength = 220.f;
	UPROPERTY() float ThirdPersonMinHold = 0.55f;
	UPROPERTY() float ThirdPersonBlendIn = 0.10f;
	UPROPERTY() float ThirdPersonBlendOut = 0.10f;
	UPROPERTY() float ViewBlendSeconds = 0.45f;

	void LoadFromIni();
};

struct CALLING_API FCLAgentLookTune
{
	float MaxYawRateDegPerSec = 420.f;
	float MaxPitchRateDegPerSec = 280.f;
	float TrackReactSeconds = 0.10f;
	float TrackMoveEpsilonCm = 40.f;
	float RecoilCorrectDelay = 0.10f;
	float RecoilCorrectPitchRate = 180.f;

	void LoadFromIni();
};

USTRUCT(BlueprintType)
struct CALLING_API FCLAbilityTune
{
	GENERATED_BODY()

	UPROPERTY() float Cooldown = 8.f;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() float Damage = 0.f;
	UPROPERTY() float Range = 0.f;
	UPROPERTY() float Radius = 0.f;
};
