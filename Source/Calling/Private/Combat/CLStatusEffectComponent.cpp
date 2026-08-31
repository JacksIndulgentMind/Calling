#include "Combat/CLStatusEffectComponent.h"
#include "AI/CLTaskMarker.h"
#include "Combat/CLDamageableComponent.h"
#include "Player/CLPlayerCharacter.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UCLStatusEffectComponent::UCLStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCLStatusEffectComponent::SetOffVolumeDot(FName InOccupyMarker, float InGraceSeconds, float InDamagePerSecond)
{
	OccupyMarker = InOccupyMarker;
	GraceSeconds = InGraceSeconds;
	DamagePerSecond = InDamagePerSecond;
	OutsideSeconds = 0.f;
	bEnabled = !InOccupyMarker.IsNone() && InDamagePerSecond > 0.f;
}

void UCLStatusEffectComponent::ClearOffVolumeDot()
{
	bEnabled = false;
	OccupyMarker = NAME_None;
	OutsideSeconds = 0.f;
}

void UCLStatusEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bEnabled)
	{
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return;
	}
	const ACLTaskMarker* Mark = ACLTaskMarker::FindById(World, OccupyMarker);
	if (!Mark)
	{
		return;
	}
	const float Zone = FMath::Max(80.f, Mark->ZoneRadiusCm);
	const bool bInside = FVector::Dist2D(Pawn->GetActorLocation(), Mark->GetActorLocation()) <= Zone;
	if (bInside)
	{
		OutsideSeconds = 0.f;
		return;
	}
	TArray<ACLTaskMarker*> Safe;
	ACLTaskMarker::CollectByTag(World, FName(TEXT("space.safe")), Safe);
	for (const ACLTaskMarker* SafeMark : Safe)
	{
		if (!SafeMark)
		{
			continue;
		}
		const float SafeZone = FMath::Max(80.f, SafeMark->ZoneRadiusCm);
		if (FVector::Dist2D(Pawn->GetActorLocation(), SafeMark->GetActorLocation()) <= SafeZone)
		{
			OutsideSeconds = 0.f;
			return;
		}
	}
	OutsideSeconds += DeltaTime;
	if (OutsideSeconds < GraceSeconds)
	{
		return;
	}
	if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Pawn))
	{
		if (UCLDamageableComponent* Dmg = Char->GetDamageable())
		{
			Dmg->ApplyDamage(DamagePerSecond * DeltaTime, nullptr, false,
				FName(TEXT("status")), FName(TEXT("offVolume")));
		}
	}
}
