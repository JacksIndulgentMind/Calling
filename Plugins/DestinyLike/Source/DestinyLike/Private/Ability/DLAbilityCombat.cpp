#include "Ability/DLAbilityCombat.h"
#include "Combat/DLDamageableComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"

namespace DLAbilityCombat
{
	float ApplyDamageToActor(AActor* Target, APawn* Instigator, float Damage)
	{
		if (!Target || Damage <= 0.f)
		{
			return 0.f;
		}
		AController* Inst = Instigator ? Instigator->GetController() : nullptr;
		if (UDLDamageableComponent* Dmg = Target->FindComponentByClass<UDLDamageableComponent>())
		{
			return Dmg->ApplyDamage(Damage, Inst, false);
		}
		if (UDLHealthShieldComponent* HS = Target->FindComponentByClass<UDLHealthShieldComponent>())
		{
			return HS->ApplyDamage(Damage, Inst, false);
		}
		return 0.f;
	}

	bool HasDamageTarget(AActor* Actor)
	{
		return Actor && (Actor->FindComponentByClass<UDLDamageableComponent>() || Actor->FindComponentByClass<UDLHealthShieldComponent>());
	}

	void ApplyDamageInRadius(UWorld* World, APawn* Instigator, const FVector& Origin, float Radius, float Damage)
	{
		if (!World || Damage <= 0.f || Radius <= 0.f)
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Other = *It;
			if (!Other || Other == Instigator || !HasDamageTarget(Other))
			{
				continue;
			}
			if (FVector::Dist(Other->GetActorLocation(), Origin) <= Radius)
			{
				ApplyDamageToActor(Other, Instigator, Damage);
			}
		}
	}

	AActor* FindNearestHostile(APawn* From, float MaxRange)
	{
		if (!From || !From->GetWorld())
		{
			return nullptr;
		}
		AActor* Best = nullptr;
		float BestDist = MaxRange;
		for (TActorIterator<AActor> It(From->GetWorld()); It; ++It)
		{
			AActor* Other = *It;
			if (!Other || Other == From || !HasDamageTarget(Other))
			{
				continue;
			}
			const float Dist = FVector::Dist(Other->GetActorLocation(), From->GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = Other;
			}
		}
		return Best;
	}

	bool TraceForward(APawn* Owner, float Distance, FHitResult& OutHit)
	{
		if (!Owner || !Owner->GetWorld())
		{
			return false;
		}
		const FVector Start = Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f);
		const FVector End = Start + Owner->GetActorForwardVector() * Distance;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DLAbilityTrace), false, Owner);
		return Owner->GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Pawn, Params)
			|| Owner->GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	}
}
