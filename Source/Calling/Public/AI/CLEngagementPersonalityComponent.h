#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/CLAIPersonalityData.h"
#include "AI/CLPersonalityStrategies.h"
#include "CLEngagementPersonalityComponent.generated.h"

UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLEngagementPersonalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLEngagementPersonalityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|AI")
	void ApplyPersonality(const FCLAIPersonalityWeight& InPersonality);

	UFUNCTION(BlueprintCallable, Category = "Calling|AI")
	void SetFocusTarget(AActor* Target);

	void FireAt(AActor* Target, float Damage, float SpreadDegrees);
	void EngagePusher();
	void EngageFlanker();
	void EngageSniper();
	void EngageGrenadier();
	void EngageAmbusher();
	void EngageCeilingShooter();
	void EngageWeaponThrower();
	void EngageIdleTroll();
	const FCLAIPersonalityWeight& GetPersonality() const { return Personality; }
	AActor* GetFocusTarget() const { return FocusTarget.Get(); }
	bool IsAmbushReady() const { return bAmbushReady; }
	void SetAmbushReady(bool bReady) { bAmbushReady = bReady; }

protected:

	UPROPERTY()
	FCLAIPersonalityWeight Personality;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;

	TSharedPtr<ICLEngageStrategy> Strategy;

	float ActionTimer = 0.f;
	bool bAmbushReady = true;
};
