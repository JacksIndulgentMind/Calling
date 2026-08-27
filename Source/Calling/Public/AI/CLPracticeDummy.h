#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CLPracticeDummy.generated.h"

class UCLHealthShieldComponent;
class UCLDamageableComponent;
class UCLEffectStackComponent;

/** Stationary (or AI-possessable) damage dummy for practice / raid grunt fallback. */
UCLASS()
class CALLING_API ACLPracticeDummy : public ACharacter
{
	GENERATED_BODY()

public:
	ACLPracticeDummy();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLHealthShieldComponent* GetHealthShield() const { return HealthShield; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLDamageableComponent* GetDamageable() const { return Damageable; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLHealthShieldComponent> HealthShield;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLDamageableComponent> Damageable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLEffectStackComponent> EffectStack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling")
	bool bStationary = true;

	UFUNCTION()
	void HandleDeath();
};
