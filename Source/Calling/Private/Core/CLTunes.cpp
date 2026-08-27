#include "Core/CLTunes.h"
#include "Misc/ConfigCacheIni.h"

void FCLTickTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLTickSettings"), TEXT("GameSimHz"), GameSimHz, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLTickSettings"), TEXT("NetHz"), NetHz, GGameIni);
	GConfig->GetBool(TEXT("/Script/Calling.CLTickSettings"), TEXT("bLockGameSimToFixedStep"), bLockGameSimToFixedStep, GGameIni);
	GameSimHz = FMath::Clamp(GameSimHz, 10.f, 120.f);
	NetHz = FMath::Clamp(NetHz, 5.f, 60.f);
}

void FCLMovementTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("BaseWalkSpeed"), BaseWalkSpeed, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("BaseStrafeSpeed"), BaseStrafeSpeed, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("SprintSpeedMultiplier"), SprintSpeedMultiplier, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("ADSMovePenalty"), ADSMovePenalty, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("SlideDuration"), SlideDuration, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("SlidePeakMultiplier"), SlidePeakMultiplier, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("SlideEndMultiplier"), SlideEndMultiplier, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("SlideAccelPortion"), SlideAccelPortion, GGameIni);
	GConfig->GetBool(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("bAllowADSWhileSliding"), bAllowADSWhileSliding, GGameIni);
	GConfig->GetBool(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("bAllowSlideDodgeCancel"), bAllowSlideDodgeCancel, GGameIni);
	GConfig->GetBool(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("bAllowSlideDashCancel"), bAllowSlideDashCancel, GGameIni);
	GConfig->GetInt(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("MaxJumps"), MaxJumps, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DashCooldownSeconds"), DashCooldownSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DashDistance"), DashDistance, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DashDuration"), DashDuration, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DashHopZ"), DashHopZ, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DodgeDistance"), DodgeDistance, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DodgeForwardScale"), DodgeForwardScale, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DodgeDuration"), DodgeDuration, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DodgeIFrames"), DodgeIFrames, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DodgeCooldownSeconds"), DodgeCooldownSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("MantleReachDistance"), MantleReachDistance, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("CrouchTransitionSeconds"), CrouchTransitionSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("CrouchHalfHeight"), CrouchHalfHeight, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveDownSpeed"), AirDiveDownSpeed, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveXYKeep"), AirDiveXYKeep, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveSteer"), AirDiveSteer, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveXYBrake"), AirDiveXYBrake, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveMaxXY"), AirDiveMaxXY, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveCooldownSeconds"), AirDiveCooldownSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("AirDiveCoalesceSeconds"), AirDiveCoalesceSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DiveHangSeconds"), DiveHangSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DiveHangGravity"), DiveHangGravity, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLMovementFeelSettings"), TEXT("DiveFallGravity"), DiveFallGravity, GGameIni);
}

void FCLCombatTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxShield"), MaxShield, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxHealth"), MaxHealth, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("ShieldRegenDelay"), ShieldRegenDelay, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("ShieldRegenPerSecond"), ShieldRegenPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("HealthRegenDelay"), HealthRegenDelay, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("HealthRegenPerSecond"), HealthRegenPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("FlinchAimPunchDegrees"), FlinchAimPunchDegrees, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("FlinchRecoveryPerSecond"), FlinchRecoveryPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RangeOptimalMinCm"), RangeOptimalMinCm, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RangeOptimalMaxCm"), RangeOptimalMaxCm, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RangeFalloffMul"), RangeFalloffMul, GGameIni);
}

void FCLWeaponMotorTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MinADSSeconds"), MinADSSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxADSSeconds"), MaxADSSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MinReadySeconds"), MinReadySeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxReadySeconds"), MaxReadySeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MinStowSeconds"), MinStowSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("MaxStowSeconds"), MaxStowSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RecoilAdsRecoverPerSecond"), RecoilAdsRecoverPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RecoilViewKickRecover"), RecoilViewKickRecover, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RecoilImpulseSeconds"), RecoilImpulseSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("HipRecoilScale"), HipRecoilScale, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("AdsRecoilScale"), AdsRecoilScale, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("RecoilRpmKnee"), RecoilRpmKnee, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("DefaultHipFOV"), DefaultHipFOV, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCombatFeelSettings"), TEXT("SlideHandlingBonus"), SlideHandlingBonus, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("AimAssistConeDegrees"), AimAssistConeDegrees, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("AimAssistMagnetism"), AimAssistMagnetism, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("ThirdPersonArmLength"), ThirdPersonArmLength, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("ThirdPersonMinHold"), ThirdPersonMinHold, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("ThirdPersonBlendIn"), ThirdPersonBlendIn, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("ThirdPersonBlendOut"), ThirdPersonBlendOut, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("ViewBlendSeconds"), ViewBlendSeconds, GGameIni);
}

void FCLAgentLookTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("MaxYawRateDegPerSec"), MaxYawRateDegPerSec, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("MaxPitchRateDegPerSec"), MaxPitchRateDegPerSec, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("TrackReactSeconds"), TrackReactSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("TrackMoveEpsilonCm"), TrackMoveEpsilonCm, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("RecoilCorrectDelay"), RecoilCorrectDelay, GGameIni);
	GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("RecoilCorrectPitchRate"), RecoilCorrectPitchRate, GGameIni);
}
