#include "Combat/DLDamageableComponent.h"
#include "Combat/DLEffectStackComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Player/DLPlayerCharacter.h"

UDLDamageableComponent::UDLDamageableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDLDamageableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		HealthShield = Owner->FindComponentByClass<UDLHealthShieldComponent>();
		EffectStack = Owner->FindComponentByClass<UDLEffectStackComponent>();
	}
	if (!HealthShield.IsValid())
	{
		Health = MaxHealth;
		bAlive = true;
	}
}

bool UDLDamageableComponent::IsAlive() const
{
	if (const UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->IsAlive();
	}
	return bAlive;
}

float UDLDamageableComponent::GetHealth() const
{
	if (const UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetHealth();
	}
	return Health;
}

float UDLDamageableComponent::GetShield() const
{
	if (const UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetShield();
	}
	return 0.f;
}

float UDLDamageableComponent::GetMaxHealth() const
{
	if (const UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetTune().MaxHealth;
	}
	return MaxHealth;
}

float UDLDamageableComponent::GetMaxShield() const
{
	if (const UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		return HS->GetTune().MaxShield;
	}
	return 0.f;
}

float UDLDamageableComponent::ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision)
{
	if (Damage <= 0.f || !IsAlive())
	{
		return 0.f;
	}
	if (const UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		if (HS->IsInvulnerable())
		{
			return 0.f;
		}
	}

	float Remaining = Damage;
	if (UDLEffectStackComponent* Stack = EffectStack.Get())
	{
		Remaining = Stack->ConsumeAbsorb(Remaining);
	}
	if (Remaining <= 0.f)
	{
		OnDamaged.Broadcast(GetHealth(), Damage);
		return Damage;
	}

	if (UDLHealthShieldComponent* HS = HealthShield.Get())
	{
		const float Applied = HS->ApplyDamage(Remaining, InstigatorController, bPrecision);
		OnDamaged.Broadcast(HS->GetHealth(), Applied);
		if (ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(GetOwner()))
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
	if (Health <= 0.f && bAlive)
	{
		bAlive = false;
		OnDeath.Broadcast();
	}
	return Applied;
}
