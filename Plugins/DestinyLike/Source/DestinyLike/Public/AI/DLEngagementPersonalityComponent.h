#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/DLAIPersonalityData.h"
#include "AI/DLPersonalityStrategies.h"
#include "DLEngagementPersonalityComponent.generated.h"

UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLEngagementPersonalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLEngagementPersonalityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|AI")
	void ApplyPersonality(const FDLAIPersonalityWeight& InPersonality);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|AI")
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
	const FDLAIPersonalityWeight& GetPersonality() const { return Personality; }
	AActor* GetFocusTarget() const { return FocusTarget.Get(); }
	bool IsAmbushReady() const { return bAmbushReady; }
	void SetAmbushReady(bool bReady) { bAmbushReady = bReady; }

protected:

	UPROPERTY()
	FDLAIPersonalityWeight Personality;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;

	TSharedPtr<IDLEngageStrategy> Strategy;

	float ActionTimer = 0.f;
	bool bAmbushReady = true;
};
