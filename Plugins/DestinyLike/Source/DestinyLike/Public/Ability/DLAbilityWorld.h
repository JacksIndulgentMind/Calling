#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DLAbilityWorld.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UCapsuleComponent;

UCLASS()
class DESTINYLIKE_API ADLAbilityAoE : public AActor
{
	GENERATED_BODY()

public:
	ADLAbilityAoE();
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
class DESTINYLIKE_API ADLAbilitySeeker : public AActor
{
	GENERATED_BODY()

public:
	ADLAbilitySeeker();
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
class DESTINYLIKE_API ADLAbilityBarricade : public AActor
{
	GENERATED_BODY()

public:
	ADLAbilityBarricade();
	void InitBarricade(float LifeSeconds);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};

UCLASS()
class DESTINYLIKE_API ADLAbilityDecoy : public AActor
{
	GENERATED_BODY()

public:
	ADLAbilityDecoy();
	void InitDecoy(float LifeSeconds);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
