#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DLPracticeDummy.generated.h"

class UDLHealthShieldComponent;
class UDLDamageableComponent;
class UDLEffectStackComponent;

/** Stationary (or AI-possessable) damage dummy for practice / raid grunt fallback. */
UCLASS()
class DESTINYLIKE_API ADLPracticeDummy : public ACharacter
{
	GENERATED_BODY()

public:
	ADLPracticeDummy();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLHealthShieldComponent* GetHealthShield() const { return HealthShield; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLDamageableComponent* GetDamageable() const { return Damageable; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLHealthShieldComponent> HealthShield;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLDamageableComponent> Damageable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLEffectStackComponent> EffectStack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike")
	bool bStationary = true;

	UFUNCTION()
	void HandleDeath();
};
