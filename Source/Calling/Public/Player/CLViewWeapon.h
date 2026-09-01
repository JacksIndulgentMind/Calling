#pragma once

#include "CoreMinimal.h"
#include "Loot/CLLootRulesService.h"

class USceneComponent;

/**
 * Greybox weapon meshes. Hitscan aim stays control rotation.
 * Tracers and casings spawn from Muzzle / Ejector on the visible family.
 * View (1P) is the lower-right lie, owner-only. World (3P) sits on the capsule, owner-no-see
 * unless a dodge/dash/dive peek shows it to the owner.
 * Same class shares one cube frame; makes change stats (and stock / muzzle parts).
 */
namespace CLViewWeapon
{
	USceneComponent* BuildOnCamera(USceneComponent* Camera);
	USceneComponent* BuildOnBody(USceneComponent* Body);
	void UpdateAdsPose(USceneComponent* WeaponRoot, float AdsAlpha, FName SightId, float KickPitch = 0.f);
	void ShowFamily(USceneComponent* WeaponRoot, FName ClassId, ECLWeaponStock Stock = ECLWeaponStock::None, bool bCompensator = false);
	void ShowSight(USceneComponent* WeaponRoot, FName SightId);
	void SetThirdPersonPeek(USceneComponent* ViewRoot, USceneComponent* WorldRoot, bool bPeek,
		FName ClassId = NAME_None, ECLWeaponStock Stock = ECLWeaponStock::None, FName SightId = NAME_None, bool bCompensator = false);
	USceneComponent* FindMuzzle(USceneComponent* WeaponRoot);
	USceneComponent* FindEjector(USceneComponent* WeaponRoot);
}
