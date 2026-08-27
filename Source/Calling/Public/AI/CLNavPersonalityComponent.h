#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/CLAIPersonalityData.h"
#include "AI/CLPersonalityStrategies.h"
#include "CLNavPersonalityComponent.generated.h"

class AAIController;

UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLNavPersonalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLNavPersonalityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|AI")
	void ApplyPersonality(const FCLAIPersonalityWeight& InPersonality);

	UFUNCTION(BlueprintCallable, Category = "Calling|AI")
	void SetFocusTarget(AActor* Target);

	void MoveToward(const FVector& WorldLocation);
	void TickWanderer(float DeltaTime);
	void TickCoverCycler(float DeltaTime);
	void TickFlanker(float DeltaTime);
	void TickHoldGround(float DeltaTime);
	void TickAggressivePush(float DeltaTime);
	void TickCircleConfused(float DeltaTime);

	const FCLAIPersonalityWeight& GetPersonality() const { return Personality; }
	AActor* GetFocusTarget() const { return FocusTarget.Get(); }
	FVector GetAnchorLocation() const { return AnchorLocation; }
	bool HasAnchor() const { return bHasAnchor; }

protected:

	UPROPERTY()
	FCLAIPersonalityWeight Personality;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;

	TSharedPtr<ICLNavStrategy> Strategy;

	float ReplanTimer = 0.f;
	FVector AnchorLocation = FVector::ZeroVector;
	bool bHasAnchor = false;
};
