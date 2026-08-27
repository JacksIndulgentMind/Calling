#pragma once

#include "CoreMinimal.h"
#include "Ability/DLAbilitySlots.h"
#include "DLAbilityConcrete.generated.h"

UCLASS()
class DESTINYLIKE_API UDLGrenade_ThrownAoE : public UDLGrenadeAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLMelee_Lunge : public UDLMeleeAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLDash_Lunge : public UDLDashAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLDash_BlinkStep : public UDLDashAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLDash_AirThrust : public UDLDashAbility
{
	GENERATED_BODY()
public:
	virtual bool CanActivate(APawn* Owner) const override;
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLJump_RocketPulse : public UDLJumpAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyToMovement(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLJump_InertiaDampers : public UDLJumpAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyToMovement(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLShield_Deployable : public UDLShieldAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class DESTINYLIKE_API UDLShield_LightADS : public UDLShieldAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class DESTINYLIKE_API UDLShield_InterceptorDrones : public UDLShieldAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class DESTINYLIKE_API UDLEvasion_Fortify : public UDLEvasionAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class DESTINYLIKE_API UDLEvasion_RippleCamo : public UDLEvasionAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class DESTINYLIKE_API UDLEvasion_Superposition : public UDLEvasionAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale) override;

protected:
	float ForesightLeft = 0.f;
	float ForesightSeconds = 0.f;
	bool bEndedSmear = false;
};

UCLASS()
class DESTINYLIKE_API UDLSuper_MindControl : public UDLSuperAbility
{
	GENERATED_BODY()
public:
	virtual bool CanActivate(APawn* Owner) const override;
	virtual bool Activate(APawn* Owner) override;
};
