#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CLAbilityWorld.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UCapsuleComponent;

UCLASS()
class CALLING_API ACLAbilityAoE : public AActor
{
	GENERATED_BODY()

public:
	ACLAbilityAoE();
	void InitAoE(APawn* InInstigator, float InRadius, float InDuration, float InImpactDamage, float InDps);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	TWeakObjectPtr<APawn> InstigatorPawn;
	float SecondsLeft = 0.f;
	float DamagePerSecond = 0.f;
	float PulseAcc = 0.f;
};

UCLASS()
class CALLING_API ACLAbilitySeeker : public AActor
{
	GENERATED_BODY()

public:
	ACLAbilitySeeker();
	void InitSeeker(APawn* InInstigator, float InDamage, float InRadius, float LifeSeconds);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	TWeakObjectPtr<APawn> InstigatorPawn;
	TWeakObjectPtr<AActor> Target;
	float Damage = 0.f;
	float ExplodeRadius = 180.f;
	float SecondsLeft = 4.f;
	void Explode();
};

UCLASS()
class CALLING_API ACLAbilityBarricade : public AActor
{
	GENERATED_BODY()

public:
	ACLAbilityBarricade();
	void InitBarricade(float LifeSeconds);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};

UCLASS()
class CALLING_API ACLAbilityDecoy : public AActor
{
	GENERATED_BODY()

public:
	ACLAbilityDecoy();
	void InitDecoy(float LifeSeconds);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
