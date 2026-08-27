#include "Ability/DLAbilityWorld.h"
#include "Ability/DLAbilityCombat.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

static UStaticMesh* DLBasicSphere()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
}

static UStaticMesh* DLBasicCube()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
}

static UStaticMesh* DLBasicCylinder()
{
	return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
}

ADLAbilityAoE::ADLAbilityAoE()
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

void ADLAbilityAoE::InitAoE(APawn* InInstigator, float InRadius, float InDuration, float InImpactDamage, float InDps)
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
		if (UStaticMesh* SphereMesh = DLBasicSphere())
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
		const float Scale = FMath::Max(0.2f, InRadius / 50.f);
		Mesh->SetWorldScale3D(FVector(Scale));
	}
	DLAbilityCombat::ApplyDamageInRadius(GetWorld(), InInstigator, GetActorLocation(), InRadius, InImpactDamage);
}

void ADLAbilityAoE::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SecondsLeft -= DeltaSeconds;
	PulseAcc += DeltaSeconds;
	if (PulseAcc >= 0.25f && Sphere)
	{
		PulseAcc = 0.f;
		DLAbilityCombat::ApplyDamageInRadius(GetWorld(), InstigatorPawn.Get(), GetActorLocation(), Sphere->GetScaledSphereRadius(), DamagePerSecond * 0.25f);
	}
	if (SecondsLeft <= 0.f)
	{
		Destroy();
	}
}

ADLAbilitySeeker::ADLAbilitySeeker()
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

void ADLAbilitySeeker::InitSeeker(APawn* InInstigator, float InDamage, float InRadius, float LifeSeconds)
{
	InstigatorPawn = InInstigator;
	Damage = InDamage;
	ExplodeRadius = InRadius;
	SecondsLeft = LifeSeconds;
	if (Mesh)
	{
		if (UStaticMesh* SphereMesh = DLBasicSphere())
		{
			Mesh->SetStaticMesh(SphereMesh);
		}
	}
	if (InInstigator)
	{
		Target = DLAbilityCombat::FindNearestHostile(InInstigator, 4000.f);
	}
}

void ADLAbilitySeeker::Tick(float DeltaSeconds)
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

void ADLAbilitySeeker::Explode()
{
	DLAbilityCombat::ApplyDamageInRadius(GetWorld(), InstigatorPawn.Get(), GetActorLocation(), ExplodeRadius, Damage);
	Destroy();
}

ADLAbilityBarricade::ADLAbilityBarricade()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Mesh->SetCastShadow(false);
}

void ADLAbilityBarricade::InitBarricade(float LifeSeconds)
{
	if (Mesh)
	{
		if (UStaticMesh* Cube = DLBasicCube())
		{
			Mesh->SetStaticMesh(Cube);
		}
		Mesh->SetWorldScale3D(FVector(0.25f, 2.4f, 2.2f));
	}
	SetLifeSpan(LifeSeconds);
}

ADLAbilityDecoy::ADLAbilityDecoy()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
}

void ADLAbilityDecoy::InitDecoy(float LifeSeconds)
{
	if (Mesh)
	{
		if (UStaticMesh* Cyl = DLBasicCylinder())
		{
			Mesh->SetStaticMesh(Cyl);
		}
		Mesh->SetWorldScale3D(FVector(0.75f, 0.75f, 1.92f));
	}
	SetLifeSpan(LifeSeconds);
}
