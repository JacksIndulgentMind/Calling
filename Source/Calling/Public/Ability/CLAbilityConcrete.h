#pragma once

#include "CoreMinimal.h"
#include "Ability/CLAbilitySlots.h"
#include "CLAbilityConcrete.generated.h"

UCLASS()
class CALLING_API UCLGrenade_ThrownAoE : public UCLGrenadeAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLMelee_Lunge : public UCLMeleeAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLDash_Lunge : public UCLDashAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLDash_BlinkStep : public UCLDashAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLDash_AirThrust : public UCLDashAbility
{
	GENERATED_BODY()
public:
	virtual bool CanActivate(APawn* Owner) const override;
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLJump_RocketPulse : public UCLJumpAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyToMovement(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLJump_InertiaDampers : public UCLJumpAbility
{
	GENERATED_BODY()
public:
	virtual void ApplyToMovement(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLShield_Deployable : public UCLShieldAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
};

UCLASS()
class CALLING_API UCLShield_LightADS : public UCLShieldAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class CALLING_API UCLShield_InterceptorDrones : public UCLShieldAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class CALLING_API UCLEvasion_Fortify : public UCLEvasionAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class CALLING_API UCLEvasion_RippleCamo : public UCLEvasionAbility
{
	GENERATED_BODY()
public:
	virtual bool Activate(APawn* Owner) override;
	virtual void Tick(float DeltaSeconds) override;
};

UCLASS()
class CALLING_API UCLEvasion_Superposition : public UCLEvasionAbility
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
class CALLING_API UCLSuper_MindControl : public UCLSuperAbility
{
	GENERATED_BODY()
public:
	virtual bool CanActivate(APawn* Owner) const override;
	virtual bool Activate(APawn* Owner) override;
};
