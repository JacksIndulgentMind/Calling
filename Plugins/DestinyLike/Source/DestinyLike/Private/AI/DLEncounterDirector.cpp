#include "AI/DLEncounterDirector.h"
#include "Core/DLLog.h"
#include "AI/DLCombatAIController.h"
#include "AI/DLPracticeDummy.h"
#include "AI/DLAIPersonalityData.h"
#include "Player/DLCombatPawn.h"
#include "Player/DLHealthShieldComponent.h"
#include "Combat/DLDamageableComponent.h"
#include "Combat/DLEffectStackComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UDLEncounterDirector::UDLEncounterDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FDLAIPersonalityWeight UDLEncounterDirector::RollDefaultPersonality() const
{
	if (PersonalityTable)
	{
		return PersonalityTable->RollPersonality();
	}
	TArray<FDLAIPersonalityWeight> FromJson;
	if (UDLAIPersonalityData::LoadDefaultJson(FromJson))
	{
		return UDLAIPersonalityData::RollFromEntries(FromJson);
	}
	FDLAIPersonalityWeight Fallback;
	Fallback.Nav = EDLNavPersonality::CoverCycler;
	Fallback.Engagement = EDLEngagementPersonality::Pusher;
	return Fallback;
}

TArray<FDLEncounterSpawnRequest> UDLEncounterDirector::BuildPlan(int32 ChamberIndex) const
{
	TArray<FDLEncounterSpawnRequest> Plan;

	// Escalate density / planning; keep HP modest.
	const int32 BaseCount = 6 + ChamberIndex * 3;
	const int32 Jitter = FMath::RandRange(-2, 4);
	const int32 Count = FMath::Clamp(BaseCount + Jitter, 4, 24);
	const int32 EliteCount = FMath::Clamp(ChamberIndex, 0, 3) + (FMath::FRand() < 0.25f ? 1 : 0);

	for (int32 i = 0; i < Count; ++i)
	{
		FDLEncounterSpawnRequest Req;
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

APawn* UDLEncounterDirector::SpawnOne(const FDLEncounterSpawnRequest& Request)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* PawnClass = Request.bElite
		? (ElitePawnClass ? *ElitePawnClass : ADLCombatPawn::StaticClass())
		: (GruntPawnClass ? *GruntPawnClass : ADLCombatPawn::StaticClass());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Pawn = World->SpawnActor<APawn>(PawnClass, Request.Location, FRotator::ZeroRotator, Params);
	if (!Pawn)
	{
		return nullptr;
	}

	if (!Pawn->FindComponentByClass<UDLHealthShieldComponent>())
	{
		UDLHealthShieldComponent* HS = NewObject<UDLHealthShieldComponent>(Pawn, TEXT("HealthShield"));
		HS->RegisterComponent();
	}
	if (!Pawn->FindComponentByClass<UDLEffectStackComponent>())
	{
		UDLEffectStackComponent* Stack = NewObject<UDLEffectStackComponent>(Pawn, TEXT("EffectStack"));
		Stack->RegisterComponent();
	}
	if (!Pawn->FindComponentByClass<UDLDamageableComponent>())
	{
		UDLDamageableComponent* Dmg = NewObject<UDLDamageableComponent>(Pawn, TEXT("Damageable"));
		Dmg->RegisterComponent();
	}

	FActorSpawnParameters AIParams;
	ADLCombatAIController* AI = World->SpawnActor<ADLCombatAIController>(ADLCombatAIController::StaticClass(), Request.Location, FRotator::ZeroRotator, AIParams);
	if (AI)
	{
		AI->Possess(Pawn);
		AI->ApplyRolledPersonality(Request.Personality);
	}
	return Pawn;
}

void UDLEncounterDirector::ClearSpawned()
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

void UDLEncounterDirector::BuildAndSpawnChamber(int32 ChamberIndex)
{
	ClearSpawned();
	const TArray<FDLEncounterSpawnRequest> Plan = BuildPlan(ChamberIndex);
	for (const FDLEncounterSpawnRequest& Req : Plan)
	{
		if (APawn* P = SpawnOne(Req))
		{
			SpawnedPawns.Add(P);
		}
	}
	UE_LOG(LogDestinyLike, Log, TEXT("DestinyLike: chamber %d spawned %d actors"), ChamberIndex, SpawnedPawns.Num());
}
