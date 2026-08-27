#pragma once

#include "CoreMinimal.h"
#include "Loot/CLLootRulesService.h"

struct FCLFireCadenceIn
{
	bool bWantsFire = false;
	bool bReady = false;
	bool bReloading = false;
	float DeltaTime = 0.f;
	float Rpm = 0.f;
	FCLWeaponFireTune Fire;
};

struct FCLFireCadenceIO
{
	bool bAwaitingFireRelease = false;
	float FireCooldown = 0.f;
	float ChargeHoldSeconds = 0.f;
	int32 BurstRemaining = 0;
	bool bBurstActive = false;
};

class ICLWeaponFireMode
{
public:
	virtual ~ICLWeaponFireMode() = default;
	virtual int32 ConsumeFire(const FCLFireCadenceIn& In, FCLFireCadenceIO& Io) = 0;
};

TSharedPtr<ICLWeaponFireMode> CLMakeWeaponFireMode(ECLWeaponFireMode Mode);
