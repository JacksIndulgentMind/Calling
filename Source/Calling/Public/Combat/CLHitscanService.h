#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class AController;
class UCLWeaponBehaviorComponent;

struct FCLHitscanRequest
{
	FVector Start = FVector::ZeroVector;
	FRotator View = FRotator::ZeroRotator;
	float Damage = 0.f;
	float RangeStat = 0.5f;
	float CritMultiplier = 1.5f;
	float SpreadDegrees = 0.f;
	float AimAssistConeDegrees = 0.f;
	float AimAssistMagnetism = 0.f;
	bool bInstantKillOnPrecision = false;
	bool bRequireAdsForInstakill = false;
	bool bIsAds = false;
	TWeakObjectPtr<UCLWeaponBehaviorComponent> Behavior;
};

struct FCLSightedTarget
{
	TWeakObjectPtr<AActor> Actor;
	float Health = 0.f;
	float Shield = 0.f;
	float MaxHealth = 100.f;
	float MaxShield = 0.f;
};

struct FCLRadarContact
{
	TWeakObjectPtr<AActor> Actor;
	FVector Location = FVector::ZeroVector;
	float DistXY = 0.f;
	float CrouchAlpha = 0.f;
	bool bLowProfile = false;
};

namespace CLHitscanService
{
	CALLING_API bool Fire(UWorld* World, AActor* InstigatorActor, AController* InstigatorController,
		const FCLHitscanRequest& Request, FRotator* OutShotRot = nullptr);

	/** One ray along punched aim, capped at RangeCm. First hit must be a living damageable. */
	CALLING_API bool QuerySightedFromPawn(const AActor* Viewer, FCLSightedTarget& OutTarget, float RangeCm = 30000.f);

	/** Living players within RangeCm that moved or fired. LOS required by default (blips); pass false for footstep ripples. */
	CALLING_API void QueryRadarContacts(const AActor* Viewer, TArray<FCLRadarContact>& OutContacts,
		float RangeCm = 3500.f, bool bRequireLos = true);

	/** Heading-up footstep mask: 1=N, 2=SE, 4=SW. Motion or gunfire + range, no LOS. */
	CALLING_API int32 QueryRadarRippleMask(const AActor* Viewer, float RangeCm = 3500.f);
}
