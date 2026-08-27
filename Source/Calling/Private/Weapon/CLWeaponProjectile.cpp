#include "Weapon/CLWeaponProjectile.h"
#include "Ability/CLAbilityCombat.h"
#include "Ability/CLAbilityWorld.h"
#include "Combat/CLDamageableComponent.h"
#include "Player/CLHealthShieldComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "CollisionQueryParams.h"

ACLWeaponProjectile::ACLWeaponProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->SetSphereRadius(8.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetGenerateOverlapEvents(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ACLWeaponProjectile::IgnoreInstigator()
{
	if (APawn* Inst = InstigatorPawn.Get())
	{
		Sphere->IgnoreActorWhenMoving(Inst, true);
		Inst->MoveIgnoreActorAdd(this);
	}
}

void ACLWeaponProjectile::InitTracer(APawn* InInstigator, const FVector& Direction, float Speed, float LifeSeconds)
{
	InstigatorPawn = InInstigator;
	bGrenade = false;
	bCasing = false;
	bProximity = false;
	SecondsLeft = LifeSeconds > 0.f ? LifeSeconds : 0.6f;
	Velocity = Direction.GetSafeNormal() * (Speed > 0.f ? Speed : 18000.f);
	IgnoreInstigator();

	if (Mesh)
	{
		if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
		Mesh->SetWorldScale3D(FVector(0.03f));
	}
	if (Sphere)
	{
		Sphere->SetSphereRadius(2.f);
	}
	SetLifeSpan(SecondsLeft + 0.05f);
}

void ACLWeaponProjectile::InitCasing(APawn* InInstigator, const FVector& Impulse)
{
	InstigatorPawn = InInstigator;
	bGrenade = false;
	bCasing = true;
	bProximity = false;
	SecondsLeft = 2.4f;
	Velocity = Impulse;
	IgnoreInstigator();

	if (Mesh)
	{
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Mesh->SetStaticMesh(CubeMesh);
		}
		Mesh->SetWorldScale3D(FVector(0.045f, 0.016f, 0.016f));
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Mat, this))
			{
				const FLinearColor Brass(0.72f, 0.52f, 0.18f, 1.f);
				Mid->SetVectorParameterValue(TEXT("Color"), Brass);
				Mid->SetVectorParameterValue(TEXT("BaseColor"), Brass);
				Mesh->SetMaterial(0, Mid);
			}
		}
	}
	if (Sphere)
	{
		Sphere->SetSphereRadius(3.f);
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}
	SetLifeSpan(SecondsLeft + 0.05f);
}

void ACLWeaponProjectile::InitGrenade(APawn* InInstigator, const FVector& Direction, float Speed, float LifeSeconds,
	float InDamage, float InRadius, bool bInProximity, float InProximityRadius)
{
	InstigatorPawn = InInstigator;
	bGrenade = true;
	bProximity = bInProximity;
	Damage = InDamage;
	ExplodeRadius = InRadius > 0.f ? InRadius : 280.f;
	ProximityRadius = InProximityRadius > 0.f ? InProximityRadius : 180.f;
	SecondsLeft = LifeSeconds > 0.f ? LifeSeconds : 6.f;
	Velocity = Direction.GetSafeNormal() * (Speed > 0.f ? Speed : 2200.f);
	IgnoreInstigator();

	if (Sphere)
	{
		Sphere->SetSphereRadius(10.f);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
	if (Mesh)
	{
		if (UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
		Mesh->SetWorldScale3D(FVector(0.16f));
	}
}

void ACLWeaponProjectile::Detonate()
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;
	if (UWorld* World = GetWorld())
	{
		if (ACLAbilityAoE* AoE = World->SpawnActor<ACLAbilityAoE>(GetActorLocation(), FRotator::ZeroRotator))
		{
			AoE->InitAoE(InstigatorPawn.Get(), ExplodeRadius, 0.28f, Damage, 0.f);
		}
		else
		{
			CLAbilityCombat::ApplyDamageInRadius(World, InstigatorPawn.Get(), GetActorLocation(), ExplodeRadius, Damage);
		}
	}
	Destroy();
}

bool ACLWeaponProjectile::CheckProximity() const
{
	if (!bProximity || !GetWorld())
	{
		return false;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Other = *It;
		if (!Other || Other == InstigatorPawn.Get())
		{
			continue;
		}
		if (!Other->FindComponentByClass<UCLDamageableComponent>()
			&& !Other->FindComponentByClass<UCLHealthShieldComponent>())
		{
			continue;
		}
		if (FVector::Dist(Other->GetActorLocation(), GetActorLocation()) <= ProximityRadius)
		{
			return true;
		}
	}
	return false;
}

void ACLWeaponProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDetonated)
	{
		return;
	}

	SecondsLeft -= DeltaSeconds;
	if (SecondsLeft <= 0.f)
	{
		if (bGrenade)
		{
			Detonate();
		}
		else
		{
			Destroy();
		}
		return;
	}

	if (bGrenade && CheckProximity())
	{
		Detonate();
		return;
	}

	if (bStopped || Velocity.IsNearlyZero())
	{
		return;
	}

	const FVector Delta = Velocity * DeltaSeconds;
	if (bCasing)
	{
		Velocity.Z += GetWorld()->GetGravityZ() * DeltaSeconds;
		FHitResult Hit;
		const FVector Start = GetActorLocation();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CLCasingFly), false, this);
		if (APawn* Inst = InstigatorPawn.Get())
		{
			Params.AddIgnoredActor(Inst);
		}
		if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + Delta, FQuat::Identity,
			ECC_WorldStatic, FCollisionShape::MakeSphere(Sphere ? Sphere->GetUnscaledSphereRadius() : 3.f), Params))
		{
			if (Hit.ImpactNormal.Z > 0.45f)
			{
				Destroy();
				return;
			}
			SetActorLocation(Hit.Location);
			Velocity = FVector::VectorPlaneProject(Velocity, Hit.ImpactNormal) * 0.35f;
			return;
		}
		SetActorLocation(Start + Delta);
		return;
	}

	if (!bGrenade)
	{
		SetActorLocation(GetActorLocation() + Delta);
		return;
	}

	FHitResult Hit;
	const FVector Start = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CLGrenadeFly), false, this);
	if (APawn* Inst = InstigatorPawn.Get())
	{
		Params.AddIgnoredActor(Inst);
	}
	if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + Delta, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(Sphere ? Sphere->GetUnscaledSphereRadius() : 10.f), Params))
	{
		SetActorLocation(Hit.Location);
		Velocity = FVector::ZeroVector;
		bStopped = true;
		return;
	}
	SetActorLocation(Start + Delta);
}
