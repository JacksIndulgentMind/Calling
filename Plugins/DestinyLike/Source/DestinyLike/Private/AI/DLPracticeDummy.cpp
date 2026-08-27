#include "AI/DLPracticeDummy.h"
#include "Player/DLHealthShieldComponent.h"
#include "Combat/DLDamageableComponent.h"
#include "Combat/DLEffectStackComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADLPracticeDummy::ADLPracticeDummy()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	HealthShield = CreateDefaultSubobject<UDLHealthShieldComponent>(TEXT("HealthShield"));
	Damageable = CreateDefaultSubobject<UDLDamageableComponent>(TEXT("Damageable"));
	EffectStack = CreateDefaultSubobject<UDLEffectStackComponent>(TEXT("EffectStack"));

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = 350.f;
	}
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ADLPracticeDummy::BeginPlay()
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
		Damageable->OnDeath.AddDynamic(this, &ADLPracticeDummy::HandleDeath);
	}
	else if (HealthShield)
	{
		HealthShield->OnDeath.AddDynamic(this, &ADLPracticeDummy::HandleDeath);
	}
}

void ADLPracticeDummy::HandleDeath()
{
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(2.f);
}
