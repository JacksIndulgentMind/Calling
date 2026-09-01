#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/CLTunes.h"
#include "CLHealthShieldComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCLOnHealthChanged, float, Health, float, Shield, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCLOnDeath);

/**
 * Regenerating shield over health.
 * Target gunfight window ~0.5-1.0s via tunable max values / TTK tables.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLHealthShieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLHealthShieldComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Server only. Client traces must not mutate replicated health. */
	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	float ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision,
		FName Kind = NAME_None, FName Source = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	void ApplyFlinch(float Strength);

	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	void ResetToFull();

	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	void RestoreShield(float Amount);

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetShield() const { return Shield; }

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetFlinchPunchDegrees() const { return CurrentFlinchPunch; }

	FVector2D ConsumeHipKick();
	FVector2D GetReticlePunch() const { return FlinchKickDir * CurrentFlinchPunch; }

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	bool IsAlive() const { return bAlive; }

	void SetInvulnerable(bool bInInvulnerable) { bInvulnerable = bInInvulnerable; }
	bool IsInvulnerable() const { return bInvulnerable; }
	void SetInterceptChance(float Chance) { InterceptChance = FMath::Clamp(Chance, 0.f, 1.f); }
	void SetDamageTakenMultiplier(float Multiplier) { DamageTakenMultiplier = FMath::Max(0.f, Multiplier); }

	UPROPERTY(BlueprintAssignable, Category = "Calling|Combat")
	FCLOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Calling|Combat")
	FCLOnDeath OnDeath;

	const FCLCombatTune& GetTune() const { return Tune; }

protected:
	void ReloadSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Combat")
	FCLCombatTune Tune;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Combat")
	float Health = 100.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Combat")
	float Shield = 100.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Calling|Combat")
	bool bAlive = true;

	float TimeSinceDamaged = 100.f;
	float CurrentFlinchPunch = 0.f;
	float FlinchResist = 0.f;
	float FlinchStability = 0.5f;
	FVector2D FlinchKickDir = FVector2D::ZeroVector;
	FVector2D PendingHipKick = FVector2D::ZeroVector;
	bool bInvulnerable = false;
	float InterceptChance = 0.f;
	float DamageTakenMultiplier = 1.f;

public:
	void SetFlinchResist(float InResist) { FlinchResist = FMath::Clamp(InResist, 0.f, 1.f); }
	void SetFlinchStability(float InStability) { FlinchStability = FMath::Clamp(InStability, 0.f, 1.5f); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
