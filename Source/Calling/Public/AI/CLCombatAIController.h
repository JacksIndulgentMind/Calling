#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/CLAIPersonalityData.h"
#include "CLCombatAIController.generated.h"

class UCLNavPersonalityComponent;
class UCLEngagementPersonalityComponent;

UCLASS()
class CALLING_API ACLCombatAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACLCombatAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|AI")
	void ApplyRolledPersonality(const FCLAIPersonalityWeight& Personality);

protected:
	void AcquireNearestPlayer();

	UPROPERTY()
	TObjectPtr<UCLNavPersonalityComponent> NavPersonality;

	UPROPERTY()
	TObjectPtr<UCLEngagementPersonalityComponent> EngagementPersonality;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	float AcquireTimer = 0.f;
};
