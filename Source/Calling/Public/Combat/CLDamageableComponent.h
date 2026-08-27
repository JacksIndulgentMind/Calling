#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CLDamageableComponent.generated.h"

class UCLHealthShieldComponent;
class UCLEffectStackComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCLOnDamaged, float, RemainingHealth, float, Applied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCLOnDamageableDeath);

/**
 * Damage entry for pawns or props. Stack absorbers first, then guardian shield/health if present,
 * otherwise this component's own health pool.
 */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLDamageableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLDamageableComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	float ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision);

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetShield() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Combat")
	float GetMaxShield() const;

	UPROPERTY(BlueprintAssignable, Category = "Calling|Combat")
	FCLOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Calling|Combat")
	FCLOnDamageableDeath OnDeath;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Combat")
	float MaxHealth = 100.f;

	UPROPERTY()
	float Health = 100.f;

	UPROPERTY()
	bool bAlive = true;

	TWeakObjectPtr<UCLHealthShieldComponent> HealthShield;
	TWeakObjectPtr<UCLEffectStackComponent> EffectStack;
};
