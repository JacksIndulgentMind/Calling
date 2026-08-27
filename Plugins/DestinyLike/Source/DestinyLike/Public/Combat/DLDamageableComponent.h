#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DLDamageableComponent.generated.h"

class UDLHealthShieldComponent;
class UDLEffectStackComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDLOnDamaged, float, RemainingHealth, float, Applied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDLOnDamageableDeath);

/**
 * Damage entry for pawns or props. Stack absorbers first, then guardian shield/health if present,
 * otherwise this component's own health pool.
 */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLDamageableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLDamageableComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Combat")
	float ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetShield() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Combat")
	float GetMaxShield() const;

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Combat")
	FDLOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Combat")
	FDLOnDamageableDeath OnDeath;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Combat")
	float MaxHealth = 100.f;

	UPROPERTY()
	float Health = 100.f;

	UPROPERTY()
	bool bAlive = true;

	TWeakObjectPtr<UDLHealthShieldComponent> HealthShield;
	TWeakObjectPtr<UDLEffectStackComponent> EffectStack;
};
