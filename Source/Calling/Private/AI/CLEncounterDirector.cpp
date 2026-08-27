#include "AI/CLEncounterDirector.h"
#include "Core/CLLog.h"
#include "AI/CLCombatAIController.h"
#include "AI/CLPracticeDummy.h"
#include "AI/CLAIPersonalityData.h"
#include "Player/CLCombatPawn.h"
#include "Player/CLHealthShieldComponent.h"
#include "Combat/CLDamageableComponent.h"
#include "Combat/CLEffectStackComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UCLEncounterDirector::UCLEncounterDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FCLAIPersonalityWeight UCLEncounterDirector::RollDefaultPersonality() const
{
	if (PersonalityTable)
	{
		return PersonalityTable->RollPersonality();
	}
	TArray<FCLAIPersonalityWeight> FromJson;
	if (UCLAIPersonalityData::LoadDefaultJson(FromJson))
	{
		return UCLAIPersonalityData::RollFromEntries(FromJson);
	}
	FCLAIPersonalityWeight Fallback;
	Fallback.Nav = ECLNavPersonality::CoverCycler;
	Fallback.Engagement = ECLEngagementPersonality::Pusher;
	return Fallback;
}

TArray<FCLEncounterSpawnRequest> UCLEncounterDirector::BuildPlan(int32 ChamberIndex) const
{
	TArray<FCLEncounterSpawnRequest> Plan;

	// Escalate density / planning; keep HP modest.
	const int32 BaseCount = 6 + ChamberIndex * 3;
	const int32 Jitter = FMath::RandRange(-2, 4);
	const int32 Count = FMath::Clamp(BaseCount + Jitter, 4, 24);
	const int32 EliteCount = FMath::Clamp(ChamberIndex, 0, 3) + (FMath::FRand() < 0.25f ? 1 : 0);

	for (int32 i = 0; i < Count; ++i)
	{
		FCLEncounterSpawnRequest Req;
		const float Ang = FMath::FRandRange(0.f, 2.f * PI);
		const float Rad = FMath::FRandRange(ArenaHalfExtent * 0.25f, ArenaHalfExtent * 0.9f);
		Req.Location = FVector(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad, 100.f);
		Req.Personality = RollDefaultPersonality();
		// Harder chambers bias smarter planning.
		Req.Personality.PlanningHorizonSeconds += 0.25f * ChamberIndex;
		Req.Personality.Aggression = FMath::Clamp(Req.Personality.Aggression + 0.08f * ChamberIndex, 0.f, 1.f);
		Req.bElite = i < EliteCount;
		if (Req.bElite)
		{
			Req.Personality.PlanningHorizonSeconds += 0.75f;
			Req.Personality.CoverDiscipline = FMath::Clamp(Req.Personality.CoverDiscipline + 0.2f, 0.f, 1.f);
		}
		Plan.Add(Req);
	}
	return Plan;
}

APawn* UCLEncounterDirector::SpawnOne(const FCLEncounterSpawnRequest& Request)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* PawnClass = Request.bElite
		? (ElitePawnClass ? *ElitePawnClass : ACLCombatPawn::StaticClass())
		: (GruntPawnClass ? *GruntPawnClass : ACLCombatPawn::StaticClass());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Pawn = World->SpawnActor<APawn>(PawnClass, Request.Location, FRotator::ZeroRotator, Params);
	if (!Pawn)
	{
		return nullptr;
	}

	if (!Pawn->FindComponentByClass<UCLHealthShieldComponent>())
	{
		UCLHealthShieldComponent* HS = NewObject<UCLHealthShieldComponent>(Pawn, TEXT("HealthShield"));
		HS->RegisterComponent();
	}
	if (!Pawn->FindComponentByClass<UCLEffectStackComponent>())
	{
		UCLEffectStackComponent* Stack = NewObject<UCLEffectStackComponent>(Pawn, TEXT("EffectStack"));
		Stack->RegisterComponent();
	}
	if (!Pawn->FindComponentByClass<UCLDamageableComponent>())
	{
		UCLDamageableComponent* Dmg = NewObject<UCLDamageableComponent>(Pawn, TEXT("Damageable"));
		Dmg->RegisterComponent();
	}

	FActorSpawnParameters AIParams;
	ACLCombatAIController* AI = World->SpawnActor<ACLCombatAIController>(ACLCombatAIController::StaticClass(), Request.Location, FRotator::ZeroRotator, AIParams);
	if (AI)
	{
		AI->Possess(Pawn);
		AI->ApplyRolledPersonality(Request.Personality);
	}
	return Pawn;
}

void UCLEncounterDirector::ClearSpawned()
{
	for (APawn* P : SpawnedPawns)
	{
		if (IsValid(P))
		{
			P->Destroy();
		}
	}
	SpawnedPawns.Reset();
}

void UCLEncounterDirector::BuildAndSpawnChamber(int32 ChamberIndex)
{
	ClearSpawned();
	const TArray<FCLEncounterSpawnRequest> Plan = BuildPlan(ChamberIndex);
	for (const FCLEncounterSpawnRequest& Req : Plan)
	{
		if (APawn* P = SpawnOne(Req))
		{
			SpawnedPawns.Add(P);
		}
	}
	UE_LOG(LogCalling, Log, TEXT("Calling: chamber %d spawned %d actors"), ChamberIndex, SpawnedPawns.Num());
}
