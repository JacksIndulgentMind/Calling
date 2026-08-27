#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DLTunes.h"
#include "DLHealthShieldComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDLOnHealthChanged, float, Health, float, Shield, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDLOnDeath);

/**
 * Halo/Destiny-like regenerating shield over health.
 * Target gunfight window ~0.5-1.0s via tunable max values / TTK tables.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLHealthShieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLHealthShieldComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Combat")
	float ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Combat")
	void ApplyFlinch(float Strength);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Combat")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Combat")
	void ResetToFull();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Combat")
	void RestoreShield(float Amount);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetShield() const { return Shield; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetFlinchPunchDegrees() const { return CurrentFlinchPunch; }

	FVector2D ConsumeHipKick();
	FVector2D GetReticlePunch() const { return FlinchKickDir * CurrentFlinchPunch; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	bool IsAlive() const { return bAlive; }

	void SetInvulnerable(bool bInInvulnerable) { bInvulnerable = bInInvulnerable; }
	bool IsInvulnerable() const { return bInvulnerable; }
	void SetInterceptChance(float Chance) { InterceptChance = FMath::Clamp(Chance, 0.f, 1.f); }
	void SetDamageTakenMultiplier(float Multiplier) { DamageTakenMultiplier = FMath::Max(0.f, Multiplier); }

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Combat")
	FDLOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Combat")
	FDLOnDeath OnDeath;

	const FDLCombatTune& GetTune() const { return Tune; }

protected:
	void ReloadSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Combat")
	FDLCombatTune Tune;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Combat")
	float Health = 100.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Combat")
	float Shield = 100.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "DestinyLike|Combat")
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
