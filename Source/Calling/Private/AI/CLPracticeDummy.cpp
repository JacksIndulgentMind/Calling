#include "AI/CLPracticeDummy.h"
#include "Player/CLHealthShieldComponent.h"
#include "Combat/CLDamageableComponent.h"
#include "Combat/CLEffectStackComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ACLPracticeDummy::ACLPracticeDummy()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	HealthShield = CreateDefaultSubobject<UCLHealthShieldComponent>(TEXT("HealthShield"));
	Damageable = CreateDefaultSubobject<UCLDamageableComponent>(TEXT("Damageable"));
	EffectStack = CreateDefaultSubobject<UCLEffectStackComponent>(TEXT("EffectStack"));

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 350.f;
	}
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ACLPracticeDummy::BeginPlay()
{
	Super::BeginPlay();
	if (bStationary)
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->DisableMovement();
		}
	}
	if (Damageable)
	{
		Damageable->OnDeath.AddDynamic(this, &ACLPracticeDummy::HandleDeath);
	}
	else if (HealthShield)
	{
		HealthShield->OnDeath.AddDynamic(this, &ACLPracticeDummy::HandleDeath);
	}
}

void ACLPracticeDummy::HandleDeath()
{
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(2.f);
}
