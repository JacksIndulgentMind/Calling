#include "AI/DLNavPersonalityComponent.h"
#include "AI/DLPersonalityStrategies.h"
#include "Player/DLPlayerCharacter.h"
#include "Input/DLAgentIntent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"

UDLNavPersonalityComponent::UDLNavPersonalityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDLNavPersonalityComponent::ApplyPersonality(const FDLAIPersonalityWeight& InPersonality)
{
	Personality = InPersonality;
	Strategy = DLMakeNavStrategy(Personality.Nav);
	ReplanTimer = 0.f;
	if (const AActor* Owner = GetOwner())
	{
		AnchorLocation = Owner->GetActorLocation();
		bHasAnchor = true;
	}
}

void UDLNavPersonalityComponent::SetFocusTarget(AActor* Target)
{
	FocusTarget = Target;
}

void UDLNavPersonalityComponent::MoveToward(const FVector& WorldLocation)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	if (ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Pawn))
	{
		const FVector Delta = WorldLocation - Pawn->GetActorLocation();
		const FVector Local = Pawn->GetActorRotation().UnrotateVector(Delta);
		FDLAgentIntent Intent;
		Intent.Move = FVector2D(Local.Y, Local.X);
		if (Intent.Move.SizeSquared() > 1.f)
		{
			Intent.Move.Normalize();
		}
		Intent.bSprint = true;
		Char->ApplyAgentIntent(Intent);
		return;
	}
	if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
	{
		AI->MoveToLocation(WorldLocation, 75.f, true, true, false, true, nullptr, true);
	}
}

void UDLNavPersonalityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ReplanTimer -= DeltaTime;
	if (ReplanTimer > 0.f)
	{
		return;
	}
	ReplanTimer = FMath::Max(0.35f, Personality.PlanningHorizonSeconds);
	if (!Strategy.IsValid())
	{
		Strategy = DLMakeNavStrategy(Personality.Nav);
	}
	if (Strategy.IsValid())
	{
		Strategy->Tick(*this, DeltaTime);
	}
}

void UDLNavPersonalityComponent::TickWanderer(float DeltaTime)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys || !GetOwner()) return;
	FNavLocation Loc;
	if (NavSys->GetRandomReachablePointInRadius(GetOwner()->GetActorLocation(), 1200.f, Loc))
	{
		MoveToward(Loc.Location);
	}
}

void UDLNavPersonalityComponent::TickCoverCycler(float DeltaTime)
{
	AActor* Target = FocusTarget.Get();
	if (!Target || !GetOwner()) return;

	const FVector Away = (GetOwner()->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(Away, FVector::UpVector).GetSafeNormal();
	const float Sign = FMath::RandBool() ? 1.f : -1.f;
	const FVector Dest = GetOwner()->GetActorLocation() + Side * Sign * (250.f + 400.f * Personality.CoverDiscipline) + Away * 150.f;
	MoveToward(Dest);
}

void UDLNavPersonalityComponent::TickFlanker(float DeltaTime)
{
	AActor* Target = FocusTarget.Get();
	if (!Target || !GetOwner()) return;

	const FVector ToTarget = (Target->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
	const FVector Side = FVector::CrossProduct(ToTarget, FVector::UpVector).GetSafeNormal();
	const float Sign = FMath::RandBool() ? 1.f : -1.f;
	const FVector Dest = Target->GetActorLocation() + Side * Sign * (400.f + 600.f * Personality.FlankBias) - ToTarget * 200.f;
	MoveToward(Dest);
}

void UDLNavPersonalityComponent::TickHoldGround(float DeltaTime)
{
	if (!bHasAnchor) return;
	MoveToward(AnchorLocation + FVector(FMath::FRandRange(-80.f, 80.f), FMath::FRandRange(-80.f, 80.f), 0.f));
}

void UDLNavPersonalityComponent::TickAggressivePush(float DeltaTime)
{
	AActor* Target = FocusTarget.Get();
	if (!Target) return;
	const FVector Dest = Target->GetActorLocation() - Target->GetActorForwardVector() * 120.f;
	MoveToward(Dest);
}

void UDLNavPersonalityComponent::TickCircleConfused(float DeltaTime)
{
	if (!GetOwner()) return;
	const float Angle = GetWorld()->GetTimeSeconds() * 2.5f;
	const FVector Dest = GetOwner()->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 300.f;
	MoveToward(Dest);
}
