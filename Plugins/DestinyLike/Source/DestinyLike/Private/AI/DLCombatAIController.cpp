#include "AI/DLCombatAIController.h"
#include "AI/DLNavPersonalityComponent.h"
#include "AI/DLEngagementPersonalityComponent.h"
#include "Player/DLPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

ADLCombatAIController::ADLCombatAIController()
{
	NavPersonality = CreateDefaultSubobject<UDLNavPersonalityComponent>(TEXT("NavPersonality"));
	EngagementPersonality = CreateDefaultSubobject<UDLEngagementPersonalityComponent>(TEXT("EngagementPersonality"));
	bWantsPlayerState = false;
}

void ADLCombatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	AcquireNearestPlayer();
}

void ADLCombatAIController::ApplyRolledPersonality(const FDLAIPersonalityWeight& Personality)
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

void ADLCombatAIController::AcquireNearestPlayer()
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

void ADLCombatAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AcquireTimer -= DeltaTime;
	if (AcquireTimer <= 0.f)
	{
		AcquireTimer = 0.75f;
		AcquireNearestPlayer();
	}
}
