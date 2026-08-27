#include "AI/CLCombatAIController.h"
#include "AI/CLNavPersonalityComponent.h"
#include "AI/CLEngagementPersonalityComponent.h"
#include "Player/CLPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

ACLCombatAIController::ACLCombatAIController()
{
	NavPersonality = CreateDefaultSubobject<UCLNavPersonalityComponent>(TEXT("NavPersonality"));
	EngagementPersonality = CreateDefaultSubobject<UCLEngagementPersonalityComponent>(TEXT("EngagementPersonality"));
	bWantsPlayerState = false;
}

void ACLCombatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AcquireNearestPlayer();
}

void ACLCombatAIController::ApplyRolledPersonality(const FCLAIPersonalityWeight& Personality)
{
	if (NavPersonality)
	{
		NavPersonality->ApplyPersonality(Personality);
	}
	if (EngagementPersonality)
	{
		EngagementPersonality->ApplyPersonality(Personality);
	}
}

void ACLCombatAIController::AcquireNearestPlayer()
{
	UWorld* World = GetWorld();
	if (!World || !GetPawn())
	{
		return;
	}

	APawn* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* P = PC->GetPawn())
			{
				const float DistSq = FVector::DistSquared(GetPawn()->GetActorLocation(), P->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = P;
				}
			}
		}
	}

	CurrentTarget = Best;
	if (NavPersonality) NavPersonality->SetFocusTarget(Best);
	if (EngagementPersonality) EngagementPersonality->SetFocusTarget(Best);
	SetFocus(Best);
}

void ACLCombatAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AcquireTimer -= DeltaTime;
	if (AcquireTimer <= 0.f)
	{
		AcquireTimer = 0.75f;
		AcquireNearestPlayer();
	}
}
