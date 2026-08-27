#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DLWeaponProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APawn;

UCLASS()
class DESTINYLIKE_API ADLWeaponProjectile : public AActor
{
	GENERATED_BODY()

public:
	ADLWeaponProjectile();

	void InitTracer(APawn* InInstigator, const FVector& Direction, float Speed, float LifeSeconds);
	void InitGrenade(APawn* InInstigator, const FVector& Direction, float Speed, float LifeSeconds,
		float InDamage, float InRadius, bool bInProximity, float InProximityRadius);
	void InitCasing(APawn* InInstigator, const FVector& Impulse);
	void Detonate();
	bool IsLiveGrenade() const { return bGrenade && !bDetonated; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	TWeakObjectPtr<APawn> InstigatorPawn;
	FVector Velocity = FVector::ZeroVector;
	float SecondsLeft = 0.f;
	float Damage = 0.f;
	float ExplodeRadius = 280.f;
	float ProximityRadius = 180.f;
	bool bGrenade = false;
	bool bProximity = false;
	bool bDetonated = false;
	bool bStopped = false;
	bool bCasing = false;

	bool CheckProximity() const;
	void IgnoreInstigator();
};
