#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CLStatusEffectComponent.generated.h"

class ACLTaskMarker;

/** Generic pawn status (DoT, etc). Not absorber stacks — those stay on UCLEffectStackComponent. */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLStatusEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLStatusEffectComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetOffVolumeDot(FName OccupyMarker, float GraceSeconds, float DamagePerSecond);
	void ClearOffVolumeDot();

protected:
	FName OccupyMarker;
	float GraceSeconds = 8.f;
	float DamagePerSecond = 0.f;
	float OutsideSeconds = 0.f;
	bool bEnabled = false;
};
