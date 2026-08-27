#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/DLAIPersonalityData.h"
#include "DLCombatAIController.generated.h"

class UDLNavPersonalityComponent;
class UDLEngagementPersonalityComponent;

UCLASS()
class DESTINYLIKE_API ADLCombatAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADLCombatAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|AI")
	void ApplyRolledPersonality(const FDLAIPersonalityWeight& Personality);

protected:
	void AcquireNearestPlayer();

	UPROPERTY()
	TObjectPtr<UDLNavPersonalityComponent> NavPersonality;

	UPROPERTY()
	TObjectPtr<UDLEngagementPersonalityComponent> EngagementPersonality;

	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentTarget;

	float AcquireTimer = 0.f;
};
