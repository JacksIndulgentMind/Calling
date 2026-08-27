#include "AI/DLEngagementPersonalityComponent.h"
#include "Core/DLLog.h"
#include "Combat/DLHitscanService.h"
#include "Player/DLHealthShieldComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

UDLEngagementPersonalityComponent::UDLEngagementPersonalityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDLEngagementPersonalityComponent::ApplyPersonality(const FDLAIPersonalityWeight& InPersonality)
{
	Personality = InPersonality;
	Strategy = DLMakeEngageStrategy(Personality.Engagement);
	ActionTimer = FMath::FRandRange(0.1f, 0.6f);
}

void UDLEngagementPersonalityComponent::SetFocusTarget(AActor* Target)
{
	FocusTarget = Target;
}

void UDLEngagementPersonalityComponent::FireAt(AActor* Target, float Damage, float SpreadDegrees)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Target || !GetWorld())
	{
		return;
	}
	FDLHitscanRequest Req;
	Req.Start = Owner->GetActorLocation() + FVector(0, 0, 60.f);
	Req.View = (Target->GetActorLocation() + FVector(0, 0, 50.f) - Req.Start).Rotation();
	Req.Damage = Damage;
	Req.SpreadDegrees = SpreadDegrees;
	AController* Instigator = Cast<APawn>(Owner) ? Cast<APawn>(Owner)->GetController() : nullptr;
	DLHitscanService::Fire(GetWorld(), Owner, Instigator, Req);
}

void UDLEngagementPersonalityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ActionTimer -= DeltaTime;
	if (ActionTimer > 0.f)
	{
		return;
	}
	if (!Strategy.IsValid())
	{
		Strategy = DLMakeEngageStrategy(Personality.Engagement);
	}
	ActionTimer = Strategy.IsValid() ? Strategy->Tick(*this) : 0.5f;
}

void UDLEngagementPersonalityComponent::EngagePusher()
{
	if (AActor* T = FocusTarget.Get()) FireAt(T, 18.f + 10.f * Personality.Aggression, 2.5f);
}

void UDLEngagementPersonalityComponent::EngageFlanker()
{
	if (AActor* T = FocusTarget.Get()) FireAt(T, 16.f, 2.0f);
}

void UDLEngagementPersonalityComponent::EngageSniper()
{
	if (AActor* T = FocusTarget.Get()) FireAt(T, 70.f, 0.6f);
}

void UDLEngagementPersonalityComponent::EngageGrenadier()
{
	AActor* T = FocusTarget.Get();
	AActor* Owner = GetOwner();
	if (!T || !Owner) return;

	// Lob stub: damage in radius near target feet.
	const FVector Center = T->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(350.f);
	if (GetWorld()->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn, Sphere))
	{
		for (const FOverlapResult& O : Overlaps)
		{
			if (UDLHealthShieldComponent* HS = O.GetActor() ? O.GetActor()->FindComponentByClass<UDLHealthShieldComponent>() : nullptr)
			{
				AController* Instigator = nullptr;
				if (APawn* OwnerPawn = Cast<APawn>(Owner))
				{
					Instigator = OwnerPawn->GetController();
				}
				HS->ApplyDamage(45.f, Instigator, false);
			}
		}
	}
}

void UDLEngagementPersonalityComponent::EngageAmbusher()
{
	AActor* T = FocusTarget.Get();
	AActor* Owner = GetOwner();
	if (!T || !Owner) return;

	const float Dist = FVector::Dist(Owner->GetActorLocation(), T->GetActorLocation());
	if (Dist < 450.f && bAmbushReady)
	{
		FireAt(T, 55.f, 1.0f);
		bAmbushReady = false;
	}
	else if (Dist > 800.f)
	{
		bAmbushReady = true;
	}
}

void UDLEngagementPersonalityComponent::EngageCeilingShooter()
{
	// Intentionally bad: shoots upward more often than at the player.
	AActor* Owner = GetOwner();
	AActor* T = FocusTarget.Get();
	if (!Owner) return;
	if (FMath::FRand() < 0.7f)
	{
		const FVector Start = Owner->GetActorLocation() + FVector(0, 0, 60.f);
		const FVector End = Start + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 2000.f);
		FHitResult Hit;
		GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);
	}
	else if (T)
	{
		FireAt(T, 12.f, 8.f);
	}
}

void UDLEngagementPersonalityComponent::EngageWeaponThrower()
{
	if (AActor* T = FocusTarget.Get())
	{
		FireAt(T, 35.f, 6.f);
		UE_LOG(LogDestinyLike, Verbose, TEXT("DestinyLike AI: weapon thrower gesture at %s"), *T->GetName());
	}
}

void UDLEngagementPersonalityComponent::EngageIdleTroll()
{
	// Rare comic relief: almost never shoots.
	if (FMath::FRand() < 0.05f)
	{
		if (AActor* T = FocusTarget.Get()) FireAt(T, 5.f, 15.f);
	}
}
