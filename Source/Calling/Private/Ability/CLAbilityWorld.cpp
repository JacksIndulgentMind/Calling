#include "Ability/CLAbilityWorld.h"
#include "Ability/CLAbilityCombat.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

static UStaticMesh* CLBasicSphere()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

static UStaticMesh* CLBasicCube()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}

static UStaticMesh* CLBasicCylinder()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
}

ACLAbilityAoE::ACLAbilityAoE()
{
	PrimaryActorTick.bCanEverTick = true;
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ACLAbilityAoE::InitAoE(APawn* InInstigator, float InRadius, float InDuration, float InImpactDamage, float InDps)
{
	InstigatorPawn = InInstigator;
	SecondsLeft = InDuration;
	DamagePerSecond = InDps;
	if (Sphere)
	{
		Sphere->SetSphereRadius(InRadius);
	}
	if (Mesh)
	{
		if (UStaticMesh* SphereMesh = CLBasicSphere())
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
		const float Scale = FMath::Max(0.2f, InRadius / 50.f);
		Mesh->SetWorldScale3D(FVector(Scale));
	}
	CLAbilityCombat::ApplyDamageInRadius(GetWorld(), InInstigator, GetActorLocation(), InRadius, InImpactDamage);
}

void ACLAbilityAoE::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SecondsLeft -= DeltaSeconds;
	PulseAcc += DeltaSeconds;
	if (PulseAcc >= 0.25f && Sphere)
	{
		PulseAcc = 0.f;
		CLAbilityCombat::ApplyDamageInRadius(GetWorld(), InstigatorPawn.Get(), GetActorLocation(), Sphere->GetScaledSphereRadius(), DamagePerSecond * 0.25f);
	}
	if (SecondsLeft <= 0.f)
	{
		Destroy();
	}
}

ACLAbilitySeeker::ACLAbilitySeeker()
{
	PrimaryActorTick.bCanEverTick = true;
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->SetSphereRadius(20.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetWorldScale3D(FVector(0.25f));
	Mesh->SetCastShadow(false);
}

void ACLAbilitySeeker::InitSeeker(APawn* InInstigator, float InDamage, float InRadius, float LifeSeconds)
{
	InstigatorPawn = InInstigator;
	Damage = InDamage;
	ExplodeRadius = InRadius;
	SecondsLeft = LifeSeconds;
	if (Mesh)
	{
		if (UStaticMesh* SphereMesh = CLBasicSphere())
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
	}
	if (InInstigator)
	{
		Target = CLAbilityCombat::FindNearestHostile(InInstigator, 4000.f);
	}
}

void ACLAbilitySeeker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SecondsLeft -= DeltaSeconds;
	AActor* Follow = Target.Get();
	if (Follow)
	{
		const FVector To = (Follow->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		SetActorLocation(GetActorLocation() + To * 900.f * DeltaSeconds);
		if (FVector::Dist(GetActorLocation(), Follow->GetActorLocation()) < 80.f)
		{
			Explode();
			return;
		}
	}
	if (SecondsLeft <= 0.f)
	{
		Explode();
	}
}

void ACLAbilitySeeker::Explode()
{
	CLAbilityCombat::ApplyDamageInRadius(GetWorld(), InstigatorPawn.Get(), GetActorLocation(), ExplodeRadius, Damage);
	Destroy();
}

ACLAbilityBarricade::ACLAbilityBarricade()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Mesh->SetCastShadow(false);
}

void ACLAbilityBarricade::InitBarricade(float LifeSeconds)
{
	if (Mesh)
	{
		if (UStaticMesh* Cube = CLBasicCube())
		{
			Mesh->SetStaticMesh(Cube);
		}
		Mesh->SetWorldScale3D(FVector(0.25f, 2.4f, 2.2f));
	}
	SetLifeSpan(LifeSeconds);
}

ACLAbilityDecoy::ACLAbilityDecoy()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ACLAbilityDecoy::InitDecoy(float LifeSeconds)
{
	if (Mesh)
	{
		if (UStaticMesh* Cyl = CLBasicCylinder())
		{
			Mesh->SetStaticMesh(Cyl);
		}
		Mesh->SetWorldScale3D(FVector(0.75f, 0.75f, 1.92f));
	}
	SetLifeSpan(LifeSeconds);
}
