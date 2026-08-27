#include "Combat/DLEffectStackComponent.h"
#include "Engine/World.h"

UDLEffectStackComponent::UDLEffectStackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDLEffectStackComponent::AddAbsorber(FName Id, float Amount, int32 Priority, float DurationSeconds)
{
	if (Amount <= 0.f)
	{
		return;
	}
	FDLDamageAbsorber Entry;
	Entry.Id = Id;
	Entry.AbsorbRemaining = Amount;
	Entry.Priority = Priority;
	if (DurationSeconds > 0.f && GetWorld())
	{
		Entry.ExpireTime = GetWorld()->GetTimeSeconds() + DurationSeconds;
	}
	Absorbers.Add(Entry);
}

float UDLEffectStackComponent::ConsumeAbsorb(float Damage)
{
	if (Damage <= 0.f || Absorbers.Num() == 0)
	{
		return Damage;
	}

	Absorbers.Sort([](const FDLDamageAbsorber& A, const FDLDamageAbsorber& B)
	{
		return A.Priority > B.Priority;
	});

	float Remaining = Damage;
	for (int32 i = 0; i < Absorbers.Num() && Remaining > 0.f; ++i)
	{
		const float Take = FMath::Min(Absorbers[i].AbsorbRemaining, Remaining);
		Absorbers[i].AbsorbRemaining -= Take;
		Remaining -= Take;
	}
	Absorbers.RemoveAll([](const FDLDamageAbsorber& E) { return E.AbsorbRemaining <= 0.f; });
	return Remaining;
}

void UDLEffectStackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld() || Absorbers.Num() == 0)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	Absorbers.RemoveAll([Now](const FDLDamageAbsorber& E)
	{
		return E.ExpireTime > 0.f && Now >= E.ExpireTime;
	});
}
