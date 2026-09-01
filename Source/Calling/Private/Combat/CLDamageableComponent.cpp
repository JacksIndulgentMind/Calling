#include "Combat/CLDamageableComponent.h"
#include "Combat/CLEffectStackComponent.h"
#include "Player/CLHealthShieldComponent.h"
#include "Player/CLPlayerCharacter.h"

UCLDamageableComponent::UCLDamageableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCLDamageableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		HealthShield = Owner->FindComponentByClass<UCLHealthShieldComponent>();
		EffectStack = Owner->FindComponentByClass<UCLEffectStackComponent>();
	}
	if (!HealthShield.IsValid())
	{
		Health = MaxHealth;
		bAlive = true;
	}
}

bool UCLDamageableComponent::IsAlive() const
{
	if (const UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->IsAlive();
	}
	return bAlive;
}

float UCLDamageableComponent::GetHealth() const
{
	if (const UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetHealth();
	}
	return Health;
}

float UCLDamageableComponent::GetShield() const
{
	if (const UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetShield();
	}
	return 0.f;
}

float UCLDamageableComponent::GetMaxHealth() const
{
	if (const UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetTune().MaxHealth;
	}
	return MaxHealth;
}

float UCLDamageableComponent::GetMaxShield() const
{
	if (const UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetTune().MaxShield;
	}
	return 0.f;
}

float UCLDamageableComponent::ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision,
	FName Kind, FName Source)
{
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		return 0.f;
	}
	if (Damage <= 0.f || !IsAlive())
	{
		return 0.f;
	}
	if (const UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		if (HS->IsInvulnerable())
		{
			return 0.f;
		}
	}

	float Remaining = Damage;
	if (UCLEffectStackComponent* Stack = EffectStack.Get())
	{
		Remaining = Stack->ConsumeAbsorb(Remaining);
	}
	if (Remaining <= 0.f)
	{
		OnDamaged.Broadcast(GetHealth(), Damage);
		return Damage;
	}

	if (UCLHealthShieldComponent* HS = HealthShield.Get())
	{
		const float Applied = HS->ApplyDamage(Remaining, InstigatorController, bPrecision, Kind, Source);
		OnDamaged.Broadcast(HS->GetHealth(), Applied);
		if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner()))
		{
			Char->NoteIncomingDamage(InstigatorController, Applied);
		}
		if (!HS->IsAlive())
		{
			OnDeath.Broadcast();
		}
		return Applied;
	}

	const float Applied = FMath::Min(Health, Remaining);
	Health -= Applied;
	OnDamaged.Broadcast(Health, Applied);
	if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner()))
	{
		Char->RecordHit(InstigatorController, Kind, Source, Applied);
		Char->NoteIncomingDamage(InstigatorController, Applied);
	}
	if (Health <= 0.f && bAlive)
	{
		bAlive = false;
		OnDeath.Broadcast();
	}
	return Applied;
}
