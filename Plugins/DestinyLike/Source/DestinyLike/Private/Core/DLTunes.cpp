#include "Core/DLTunes.h"
#include "Misc/ConfigCacheIni.h"

void FDLTickTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLTickSettings"), TEXT("GameSimHz"), GameSimHz, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLTickSettings"), TEXT("NetHz"), NetHz, GGameIni);
	GConfig->GetBool(TEXT("/Script/DestinyLike.DLTickSettings"), TEXT("bLockGameSimToFixedStep"), bLockGameSimToFixedStep, GGameIni);
	GameSimHz = FMath::Clamp(GameSimHz, 10.f, 120.f);
	NetHz = FMath::Clamp(NetHz, 5.f, 60.f);
}

void FDLMovementTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("BaseWalkSpeed"), BaseWalkSpeed, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("BaseStrafeSpeed"), BaseStrafeSpeed, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("SprintSpeedMultiplier"), SprintSpeedMultiplier, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("ADSMovePenalty"), ADSMovePenalty, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("SlideDuration"), SlideDuration, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("SlidePeakMultiplier"), SlidePeakMultiplier, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("SlideEndMultiplier"), SlideEndMultiplier, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("SlideAccelPortion"), SlideAccelPortion, GGameIni);
	GConfig->GetBool(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("bAllowADSWhileSliding"), bAllowADSWhileSliding, GGameIni);
	GConfig->GetBool(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("bAllowSlideDodgeCancel"), bAllowSlideDodgeCancel, GGameIni);
	GConfig->GetBool(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("bAllowSlideDashCancel"), bAllowSlideDashCancel, GGameIni);
	GConfig->GetInt(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("MaxJumps"), MaxJumps, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DashCooldownSeconds"), DashCooldownSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DashDistance"), DashDistance, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DashDuration"), DashDuration, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DashHopZ"), DashHopZ, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DodgeDistance"), DodgeDistance, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DodgeForwardScale"), DodgeForwardScale, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DodgeDuration"), DodgeDuration, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DodgeIFrames"), DodgeIFrames, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DodgeCooldownSeconds"), DodgeCooldownSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("MantleReachDistance"), MantleReachDistance, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("CrouchTransitionSeconds"), CrouchTransitionSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("CrouchHalfHeight"), CrouchHalfHeight, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveDownSpeed"), AirDiveDownSpeed, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveXYKeep"), AirDiveXYKeep, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveSteer"), AirDiveSteer, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveXYBrake"), AirDiveXYBrake, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveMaxXY"), AirDiveMaxXY, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveCooldownSeconds"), AirDiveCooldownSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("AirDiveCoalesceSeconds"), AirDiveCoalesceSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DiveHangSeconds"), DiveHangSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DiveHangGravity"), DiveHangGravity, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DiveFallGravity"), DiveFallGravity, GGameIni);
}

void FDLCombatTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxShield"), MaxShield, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxHealth"), MaxHealth, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("ShieldRegenDelay"), ShieldRegenDelay, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("ShieldRegenPerSecond"), ShieldRegenPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("HealthRegenDelay"), HealthRegenDelay, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("HealthRegenPerSecond"), HealthRegenPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("FlinchAimPunchDegrees"), FlinchAimPunchDegrees, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("FlinchRecoveryPerSecond"), FlinchRecoveryPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RangeOptimalMinCm"), RangeOptimalMinCm, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RangeOptimalMaxCm"), RangeOptimalMaxCm, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RangeFalloffMul"), RangeFalloffMul, GGameIni);
}

void FDLWeaponMotorTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MinADSSeconds"), MinADSSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxADSSeconds"), MaxADSSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MinReadySeconds"), MinReadySeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxReadySeconds"), MaxReadySeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MinStowSeconds"), MinStowSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("MaxStowSeconds"), MaxStowSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RecoilAdsRecoverPerSecond"), RecoilAdsRecoverPerSecond, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RecoilViewKickRecover"), RecoilViewKickRecover, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RecoilImpulseSeconds"), RecoilImpulseSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("HipRecoilScale"), HipRecoilScale, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("AdsRecoilScale"), AdsRecoilScale, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("RecoilRpmKnee"), RecoilRpmKnee, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("DefaultHipFOV"), DefaultHipFOV, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCombatFeelSettings"), TEXT("SlideHandlingBonus"), SlideHandlingBonus, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("AimAssistConeDegrees"), AimAssistConeDegrees, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("AimAssistMagnetism"), AimAssistMagnetism, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("ThirdPersonArmLength"), ThirdPersonArmLength, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("ThirdPersonMinHold"), ThirdPersonMinHold, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("ThirdPersonBlendIn"), ThirdPersonBlendIn, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("ThirdPersonBlendOut"), ThirdPersonBlendOut, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("ViewBlendSeconds"), ViewBlendSeconds, GGameIni);
}

void FDLAgentLookTune::LoadFromIni()
{
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("MaxYawRateDegPerSec"), MaxYawRateDegPerSec, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("MaxPitchRateDegPerSec"), MaxPitchRateDegPerSec, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("TrackReactSeconds"), TrackReactSeconds, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("TrackMoveEpsilonCm"), TrackMoveEpsilonCm, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("RecoilCorrectDelay"), RecoilCorrectDelay, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("RecoilCorrectPitchRate"), RecoilCorrectPitchRate, GGameIni);
}
