#pragma once

#include "CoreMinimal.h"

class USceneComponent;

/**
 * Greybox weapon meshes. Hitscan aim stays control rotation.
 * Tracers and casings spawn from Muzzle / Ejector on the visible family.
 * View (1P) is the lower-right lie, owner-only. World (3P) sits on the capsule, owner-no-see
 * unless a dodge/dash/dive peek shows it to the owner.
 */
namespace DLViewWeapon
{
	USceneComponent* BuildOnCamera(USceneComponent* Camera);
	USceneComponent* BuildOnBody(USceneComponent* Body);
	void UpdateAdsPose(USceneComponent* WeaponRoot, float AdsAlpha, FName SightId, float KickPitch = 0.f);
	void ShowFamily(USceneComponent* WeaponRoot, bool bGrenadeRifle);
	void ShowSight(USceneComponent* WeaponRoot, FName SightId);
	void SetThirdPersonPeek(USceneComponent* ViewRoot, USceneComponent* WorldRoot, bool bPeek,
		bool bGrenadeRifle = false, FName SightId = NAME_None);
	USceneComponent* FindMuzzle(USceneComponent* WeaponRoot);
	USceneComponent* FindEjector(USceneComponent* WeaponRoot);
}
