#pragma once

#include "CoreMinimal.h"
#include "Ability/DLAbility.h"
#include "DLAbilitySlots.generated.h"

UCLASS(Abstract)
class DESTINYLIKE_API UDLGrenadeAbility : public UDLAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

	UPROPERTY() float LingerSeconds = 0.f;
	UPROPERTY() float BurnDamagePerSecond = 0.f;
	UPROPERTY() bool bBurn = false;
	UPROPERTY() bool bLinger = false;
	UPROPERTY() bool bSpawnSeekers = false;
	UPROPERTY() int32 SeekerCount = 0;
};

UCLASS(Abstract)
class DESTINYLIKE_API UDLShieldAbility : public UDLAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

	UPROPERTY() float HoverSeconds = 0.f;
	UPROPERTY() float InterceptChance = 0.f;
};

UCLASS(Abstract)
class DESTINYLIKE_API UDLEvasionAbility : public UDLAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

	UPROPERTY() float DamageTakenMultiplier = 1.f;
};

UCLASS(Abstract)
class DESTINYLIKE_API UDLDashAbility : public UDLAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

	UPROPERTY() float AirControl = -1.f;
	UPROPERTY() float HoverSeconds = 0.f;
};

UCLASS(Abstract)
class DESTINYLIKE_API UDLMeleeAbility : public UDLAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

	UPROPERTY() float HealSelf = 0.f;
};

UCLASS(Abstract)
class DESTINYLIKE_API UDLJumpAbility : public UDLAbility
{
	GENERATED_BODY()
public:
	virtual bool CanActivate(APawn* Owner) const override;
	virtual bool Activate(APawn* Owner) override;
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

	UPROPERTY() float AirControl = -1.f;
	UPROPERTY() float SecondJumpZ = 0.f;
	UPROPERTY() float HoverSeconds = 0.f;
	UPROPERTY() bool bAllowSecondJumpFromGround = false;
};

UCLASS(Abstract)
class DESTINYLIKE_API UDLSuperAbility : public UDLAbility
{
	GENERATED_BODY()
};
