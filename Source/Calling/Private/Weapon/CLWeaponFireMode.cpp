#include "Weapon/CLWeaponFireMode.h"

namespace
{
	float ShotGap(const FCLFireCadenceIn& In)
	{
		if (In.Rpm > 0.f)
		{
			return 60.f / In.Rpm;
		}
		return FMath::Max(0.05f, In.Fire.ChargeSeconds);
	}

	class FHitscanFireMode final : public ICLWeaponFireMode
	{
	public:
		virtual int32 ConsumeFire(const FCLFireCadenceIn& In, FCLFireCadenceIO& Io) override
		{
			if (!In.bWantsFire || Io.bAwaitingFireRelease || Io.FireCooldown > 0.f || !In.bReady || In.bReloading)
			{
				return 0;
			}
			Io.FireCooldown = ShotGap(In);
			return 1;
		}
	};

	class FGrenadeFireMode final : public ICLWeaponFireMode
	{
	public:
		virtual int32 ConsumeFire(const FCLFireCadenceIn& In, FCLFireCadenceIO& Io) override
		{
			if (!In.bWantsFire || Io.bAwaitingFireRelease || Io.FireCooldown > 0.f || !In.bReady || In.bReloading)
			{
				return 0;
			}
			Io.FireCooldown = ShotGap(In);
			Io.bAwaitingFireRelease = true;
			return 1;
		}
	};

	class FChargeFireMode final : public ICLWeaponFireMode
	{
	public:
		virtual int32 ConsumeFire(const FCLFireCadenceIn& In, FCLFireCadenceIO& Io) override
		{
			if (!In.bWantsFire)
			{
				Io.ChargeHoldSeconds = 0.f;
				return 0;
			}
			if (Io.FireCooldown > 0.f || !In.bReady || In.bReloading)
			{
				return 0;
			}
			Io.ChargeHoldSeconds += In.DeltaTime;
			if (Io.ChargeHoldSeconds < FMath::Max(0.05f, In.Fire.ChargeSeconds))
			{
				return 0;
			}
			Io.ChargeHoldSeconds = 0.f;
			Io.FireCooldown = 0.f;
			return 1;
		}
	};

	class FBurstFireMode final : public ICLWeaponFireMode
	{
	public:
		virtual int32 ConsumeFire(const FCLFireCadenceIn& In, FCLFireCadenceIO& Io) override
		{
			if (Io.bBurstActive)
			{
				if (Io.FireCooldown > 0.f || !In.bReady || In.bReloading)
				{
					return 0;
				}
				--Io.BurstRemaining;
				Io.FireCooldown = ShotGap(In);
				if (Io.BurstRemaining <= 0)
				{
					Io.bBurstActive = false;
					Io.bAwaitingFireRelease = true;
				}
				return 1;
			}
			if (!In.bWantsFire || Io.bAwaitingFireRelease || Io.FireCooldown > 0.f || !In.bReady || In.bReloading)
			{
				return 0;
			}
			const int32 Count = FMath::Max(1, In.Fire.BurstCount);
			Io.FireCooldown = ShotGap(In);
			if (Count > 1)
			{
				Io.bBurstActive = true;
				Io.BurstRemaining = Count - 1;
			}
			else
			{
				Io.bAwaitingFireRelease = true;
			}
			return 1;
		}
	};
}

TSharedPtr<ICLWeaponFireMode> CLMakeWeaponFireMode(ECLWeaponFireMode Mode)
{
	switch (Mode)
	{
	case ECLWeaponFireMode::Pellet: return MakeShared<FHitscanFireMode>();
	case ECLWeaponFireMode::Charge: return MakeShared<FChargeFireMode>();
	case ECLWeaponFireMode::Grenade: return MakeShared<FGrenadeFireMode>();
	case ECLWeaponFireMode::Burst: return MakeShared<FBurstFireMode>();
	default: return MakeShared<FHitscanFireMode>();
	}
}
