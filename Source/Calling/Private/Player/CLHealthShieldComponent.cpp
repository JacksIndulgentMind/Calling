#include "Player/CLHealthShieldComponent.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLPlayerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/Controller.h"

UCLHealthShieldComponent::UCLHealthShieldComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCLHealthShieldComponent::BeginPlay()
{
	Super::BeginPlay();
	ReloadSettings();
	Health = Tune.MaxHealth;
	Shield = Tune.MaxShield;
	bAlive = true;
}

void UCLHealthShieldComponent::ReloadSettings()
{
	Tune.LoadFromIni();
}

void UCLHealthShieldComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCLHealthShieldComponent, Health);
	DOREPLIFETIME(UCLHealthShieldComponent, Shield);
	DOREPLIFETIME(UCLHealthShieldComponent, bAlive);
}

float UCLHealthShieldComponent::ApplyDamage(float Damage, AController* InstigatorController, bool bPrecision,
	FName Kind, FName Source)
{
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		return 0.f;
	}
	if (!bAlive || Damage <= 0.f || bInvulnerable)
	{
		return 0.f;
	}
	if (InterceptChance > 0.f && FMath::FRand() < InterceptChance)
	{
		return 0.f;
	}

	float Remaining = Damage * DamageTakenMultiplier;
	float Applied = 0.f;

	if (Shield > 0.f)
	{
		const float ToShield = FMath::Min(Shield, Remaining);
		Shield -= ToShield;
		Remaining -= ToShield;
		Applied += ToShield;
	}
	if (Remaining > 0.f)
	{
		const float ToHealth = FMath::Min(Health, Remaining);
		Health -= ToHealth;
		Applied += ToHealth;
	}

	TimeSinceDamaged = 0.f;
	ApplyFlinch(bPrecision ? 1.25f : 1.f);
	OnHealthChanged.Broadcast(Health, Shield, Applied);

	if (Applied > 0.f)
	{
		if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetOwner()))
		{
			Char->RecordHit(InstigatorController, Kind, Source, Applied);
		}
	}

	if (Health <= 0.f)
	{
		bAlive = false;
		OnDeath.Broadcast();
	}
	return Applied;
}

void UCLHealthShieldComponent::ApplyFlinch(float Strength)
{
	float Posture = 1.f;
	if (const AActor* Owner = GetOwner())
	{
		if (const UCLCombatMovementComponent* Move = Owner->FindComponentByClass<UCLCombatMovementComponent>())
		{
			if (Move->IsSliding())
			{
				Posture = 0.45f;
			}
			else if (Move->GetCrouchAlpha() > 0.35f)
			{
				Posture = 0.65f;
			}
		}
	}
	const float ResistMul = (1.f - FlinchResist) * (1.f - FMath::Clamp(FlinchStability, 0.f, 1.f)) * Posture;
	const float Mag = Tune.FlinchAimPunchDegrees * Strength * ResistMul;
	CurrentFlinchPunch = FMath::Min(CurrentFlinchPunch + Mag, Tune.FlinchAimPunchDegrees * 3.f);
	FlinchKickDir = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal();
	if (FlinchKickDir.IsNearlyZero())
	{
		FlinchKickDir = FVector2D(1.f, 0.35f).GetSafeNormal();
	}
	PendingHipKick += FlinchKickDir * Mag;
}

FVector2D UCLHealthShieldComponent::ConsumeHipKick()
{
	const FVector2D Kick = PendingHipKick;
	PendingHipKick = FVector2D::ZeroVector;
	return Kick;
}

void UCLHealthShieldComponent::Heal(float Amount)
{
	if (!bAlive) return;
	Health = FMath::Min(Tune.MaxHealth, Health + Amount);
	OnHealthChanged.Broadcast(Health, Shield, 0.f);
}

void UCLHealthShieldComponent::ResetToFull()
{
	Health = Tune.MaxHealth;
	Shield = Tune.MaxShield;
	bAlive = true;
	TimeSinceDamaged = 100.f;
	CurrentFlinchPunch = 0.f;
	FlinchKickDir = FVector2D::ZeroVector;
	PendingHipKick = FVector2D::ZeroVector;
	OnHealthChanged.Broadcast(Health, Shield, 0.f);
}

void UCLHealthShieldComponent::RestoreShield(float Amount)
{
	if (!bAlive) return;
	Shield = FMath::Min(Tune.MaxShield, Shield + Amount);
	OnHealthChanged.Broadcast(Health, Shield, 0.f);
}

void UCLHealthShieldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bAlive)
	{
		return;
	}

	TimeSinceDamaged += DeltaTime;
	bool bChanged = false;
	if (TimeSinceDamaged >= Tune.ShieldRegenDelay && Shield < Tune.MaxShield)
	{
		Shield = FMath::Min(Tune.MaxShield, Shield + Tune.ShieldRegenPerSecond * DeltaTime);
		bChanged = true;
	}
	if (TimeSinceDamaged >= Tune.HealthRegenDelay && Health < Tune.MaxHealth)
	{
		Health = FMath::Min(Tune.MaxHealth, Health + Tune.HealthRegenPerSecond * DeltaTime);
		bChanged = true;
	}
	if (bChanged)
	{
		OnHealthChanged.Broadcast(Health, Shield, 0.f);
	}

	CurrentFlinchPunch = FMath::Max(0.f, CurrentFlinchPunch - Tune.FlinchRecoveryPerSecond * DeltaTime);
}
