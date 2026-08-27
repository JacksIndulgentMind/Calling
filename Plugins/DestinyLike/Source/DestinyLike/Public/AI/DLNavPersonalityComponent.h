#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/DLAIPersonalityData.h"
#include "AI/DLPersonalityStrategies.h"
#include "DLNavPersonalityComponent.generated.h"

class AAIController;

UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLNavPersonalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLNavPersonalityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|AI")
	void ApplyPersonality(const FDLAIPersonalityWeight& InPersonality);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|AI")
	void SetFocusTarget(AActor* Target);

	void MoveToward(const FVector& WorldLocation);
	void TickWanderer(float DeltaTime);
	void TickCoverCycler(float DeltaTime);
	void TickFlanker(float DeltaTime);
	void TickHoldGround(float DeltaTime);
	void TickAggressivePush(float DeltaTime);
	void TickCircleConfused(float DeltaTime);

	const FDLAIPersonalityWeight& GetPersonality() const { return Personality; }
	AActor* GetFocusTarget() const { return FocusTarget.Get(); }
	FVector GetAnchorLocation() const { return AnchorLocation; }
	bool HasAnchor() const { return bHasAnchor; }

protected:

	UPROPERTY()
	FDLAIPersonalityWeight Personality;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;

	TSharedPtr<IDLNavStrategy> Strategy;

	float ReplanTimer = 0.f;
	FVector AnchorLocation = FVector::ZeroVector;
	bool bHasAnchor = false;
};
