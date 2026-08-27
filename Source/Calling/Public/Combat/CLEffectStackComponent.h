#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CLEffectStackComponent.generated.h"

USTRUCT(BlueprintType)
struct CALLING_API FCLDamageAbsorber
{
	GENERATED_BODY()

	UPROPERTY()
	FName Id;

	UPROPERTY()
	float AbsorbRemaining = 0.f;

	UPROPERTY()
	int32 Priority = 0;

	UPROPERTY()
	float ExpireTime = 0.f;
};

/** Temporary shields and similar: consume incoming damage until depleted. */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLEffectStackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLEffectStackComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Combat")
	void AddAbsorber(FName Id, float Amount, int32 Priority = 0, float DurationSeconds = 0.f);

	/** Consumes absorbers (high priority first). Returns leftover damage. */
	float ConsumeAbsorb(float Damage);

protected:
	UPROPERTY()
	TArray<FCLDamageAbsorber> Absorbers;
};
