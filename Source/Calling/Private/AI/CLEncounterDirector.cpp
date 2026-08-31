#include "AI/CLEncounterDirector.h"
#include "AI/CLBotDefCatalog.h"
#include "Core/CLTypes.h"
#include "AI/CLBotBookManager.h"
#include "AI/CLCombatAIController.h"
#include "AI/CLTaskMarker.h"
#include "Combat/CLStatusEffectComponent.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLGreyboxFloors.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSeatMotor.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Player/CLCombatPawn.h"
#include "Player/CLLookController.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLPossessionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "WorldCollision.h"
#include "GameFramework/Pawn.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "NavMesh/RecastNavMesh.h"
#include "Kismet/GameplayStatics.h"

UCLEncounterDirector::UCLEncounterDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCLEncounterDirector::BindWaveHold(const TArray<const FCLWaveHoldEncounter*>& Encounters)
{
	WaveHolds = Encounters;
	EncounterIndex = 0;
	PhaseIndex = 0;
	bRunning = false;
	bFinished = false;
}

void UCLEncounterDirector::BeginFirstEncounter()
{
	if (WaveHolds.Num() == 0)
	{
		bFinished = true;
		return;
	}
	bAwaitingNav = true;
	bRunning = true;
	bFinished = false;
}

bool UCLEncounterDirector::HasNavTiles() const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	const ARecastNavMesh* Recast = NavSys
		? Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance(FNavigationSystem::ECreateIfMissing::DontCreate))
		: nullptr;
	return Recast && Recast->GetNumActiveTiles() > 0;
}

void UCLEncounterDirector::ClearSpawned()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UCLBotBookManager* Books = GI ? GI->GetSubsystem<UCLBotBookManager>() : nullptr;
	for (FSpawnedNpc& N : Spawned)
	{
		if (N.Seat && Books)
		{
			Books->ClearSeat(N.Seat->GetSeatId());
		}
		if (APawn* P = N.Pawn.Get())
		{
			P->Destroy();
		}
	}
	Spawned.Reset();
	NpcSeats.Reset();
}

void UCLEncounterDirector::StartEncounter(int32 Index)
{
	ClearSpawned();
	EncounterIndex = Index;
	PhaseIndex = 0;
	bRunning = true;
	if (!WaveHolds.IsValidIndex(EncounterIndex))
	{
		bFinished = true;
		bRunning = false;
		return;
	}
	if (ACLRaidGameMode* Raid = Cast<ACLRaidGameMode>(GetOwner()))
	{
		Raid->NotifyEncounterBegin(EncounterIndex);
	}
	StartPhase(0);
}

void UCLEncounterDirector::StartPhase(int32 InPhase)
{
	PhaseIndex = InPhase;
	WavesDone = 0;
	WavesSpawned = 0;
	bWaveInFlight = false;
	WaveTimer = 0.f;
	const FCLWaveHoldEncounter* Enc = WaveHolds.IsValidIndex(EncounterIndex) ? WaveHolds[EncounterIndex] : nullptr;
	if (!Enc || !Enc->Phases.IsValidIndex(PhaseIndex))
	{
		CompleteEncounter();
		return;
	}
	const FCLWaveHoldPhase& Phase = Enc->Phases[PhaseIndex];
	if (UWorld* World = GetWorld())
	{
		if (ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
		{
			GS->SetLiveShrine(Phase.OccupyMarker);
			GS->SetEncounterProgress(Enc->Id, Phase.Id, 0);
		}
	}
	ApplyOffVolume(Phase);
	SpawnWave();
}

void UCLEncounterDirector::ApplyOffVolume(const FCLWaveHoldPhase& Phase)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		UCLStatusEffectComponent* Status = EnsureStatus(Pawn);
		if (!Status)
		{
			continue;
		}
		if (Phase.OffVolume.bEnabled)
		{
			Status->SetOffVolumeDot(Phase.OccupyMarker, Phase.OffVolume.GraceSeconds, Phase.OffVolume.DamagePerSecond);
		}
		else
		{
			Status->ClearOffVolumeDot();
		}
	}
}

UCLStatusEffectComponent* UCLEncounterDirector::EnsureStatus(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}
	if (UCLStatusEffectComponent* Existing = Pawn->FindComponentByClass<UCLStatusEffectComponent>())
	{
		return Existing;
	}
	UCLStatusEffectComponent* Status = NewObject<UCLStatusEffectComponent>(Pawn, TEXT("StatusEffect"));
	Status->RegisterComponent();
	return Status;
}

FName UCLEncounterDirector::RollBot(const FCLSpawnerDef& Spawner) const
{
	float Sum = 0.f;
	for (const FCLSpawnerPoolEntry& E : Spawner.Pool)
	{
		Sum += FMath::Max(0.f, E.Weight);
	}
	if (Sum <= 0.f || Spawner.Pool.Num() == 0)
	{
		return NAME_None;
	}
	float Pick = FMath::FRandRange(0.f, Sum);
	for (const FCLSpawnerPoolEntry& E : Spawner.Pool)
	{
		Pick -= FMath::Max(0.f, E.Weight);
		if (Pick <= 0.f)
		{
			return E.Bot;
		}
	}
	return Spawner.Pool.Last().Bot;
}

bool UCLEncounterDirector::FindClearSpawnLocation(const FVector& Origin, const FCLSpawnerDef& Spawner, FVector& OutStand, FString& OutReason) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = TEXT("no_world");
		return false;
	}
	float CapsuleR = 50.f;
	float HalfH = 96.f;
	if (const ACLCombatPawn* CDO = GetDefault<ACLCombatPawn>())
	{
		if (const UCapsuleComponent* Cap = CDO->GetCapsuleComponent())
		{
			CapsuleR = Cap->GetUnscaledCapsuleRadius();
			HalfH = Cap->GetUnscaledCapsuleHalfHeight();
		}
	}
	if (Spawner.ClearRadiusCm > 0.f)
	{
		CapsuleR = Spawner.ClearRadiusCm;
	}
	float Radius = Spawner.RadiusCm;
	if (Radius <= 0.f)
	{
		if (const ACLTaskMarker* Mark = ACLTaskMarker::FindById(World, Spawner.OriginMarker))
		{
			Radius = Mark->ZoneRadiusCm;
		}
	}
	const int32 Tries = Spawner.ClearTries > 0 ? Spawner.ClearTries : 12;
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	OutReason = TEXT("exhausted");
	for (int32 i = 0; i < Tries; ++i)
	{
		FVector2D Offset = FVector2D::ZeroVector;
		if (i > 0 && Radius > 0.f)
		{
			const float Ang = FMath::FRandRange(0.f, 2.f * PI);
			const float R = Radius * FMath::Sqrt(FMath::FRand());
			Offset = FVector2D(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R);
		}
		const FVector Top = Origin + FVector(Offset.X, Offset.Y, 280.f);
		const FVector Bot = Origin + FVector(Offset.X, Offset.Y, -500.f);
		FHitResult Floor;
		FCollisionQueryParams TraceQ(SCENE_QUERY_STAT(CLSpawnFloor), false);
		if (!World->LineTraceSingleByChannel(Floor, Top, Bot, ECC_WorldStatic, TraceQ) || !Floor.bBlockingHit)
		{
			OutReason = TEXT("no_floor");
			continue;
		}
		if (Floor.ImpactNormal.Z < 0.35f)
		{
			OutReason = TEXT("steep");
			continue;
		}
		FVector Stand = Floor.ImpactPoint + FVector(0.f, 0.f, HalfH + 4.f);
		if (NavSys)
		{
			FNavLocation NavLoc;
			if (!NavSys->ProjectPointToNavigation(Stand, NavLoc, FVector(CapsuleR * 2.f, CapsuleR * 2.f, 250.f)))
			{
				OutReason = TEXT("no_nav");
				continue;
			}
			Stand = FVector(NavLoc.Location.X, NavLoc.Location.Y, NavLoc.Location.Z + HalfH + 4.f);
		}
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(CapsuleR, HalfH);
		FCollisionQueryParams OverQ(SCENE_QUERY_STAT(CLSpawnClear), false);
		TArray<FOverlapResult> Hits;
		World->OverlapMultiByChannel(Hits, Stand, FQuat::Identity, ECC_Pawn, Shape, OverQ);
		bool bBlocked = false;
		for (const FOverlapResult& H : Hits)
		{
			if (Cast<APawn>(H.GetActor()))
			{
				bBlocked = true;
				break;
			}
		}
		if (bBlocked)
		{
			OutReason = TEXT("pawn");
			continue;
		}
		Hits.Reset();
		World->OverlapMultiByChannel(Hits, Stand, FQuat::Identity, ECC_WorldStatic, Shape, OverQ);
		for (const FOverlapResult& H : Hits)
		{
			if (H.GetComponent() == Floor.GetComponent())
			{
				continue;
			}
			if (UPrimitiveComponent* Prim = H.GetComponent())
			{
				const FBox Box = Prim->Bounds.GetBox();
				const float Height = Box.Max.Z - Box.Min.Z;
				if (Height < 80.f && Box.Max.Z < Stand.Z - HalfH + 20.f)
				{
					continue;
				}
			}
			bBlocked = true;
			break;
		}
		if (bBlocked)
		{
			OutReason = TEXT("wall");
			continue;
		}
		OutStand = Stand;
		OutReason.Reset();
		return true;
	}
	return false;
}

APawn* UCLEncounterDirector::SpawnBot(FName BotId, const FVector& Location)
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!World || !GI)
	{
		return nullptr;
	}
	UCLBotDefCatalog* Bots = GI->GetSubsystem<UCLBotDefCatalog>();
	const FCLBotDef* Def = Bots ? Bots->Find(BotId) : nullptr;
	if (!Def)
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::Logic, TEXT("raid_unknown_bot"), BotId.ToString()));
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	ACLCombatPawn* Pawn = World->SpawnActor<ACLCombatPawn>(ACLCombatPawn::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Pawn)
	{
		UE_LOG(LogCalling, Error, TEXT("SpawnBot collide bot=%s loc=(%.0f,%.0f,%.0f)"),
			*BotId.ToString(), Location.X, Location.Y, Location.Z);
		return nullptr;
	}
	Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
	if (AController* Old = Pawn->GetController())
	{
		Old->UnPossess();
	}
	FActorSpawnParameters AIParams;
	AIParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACLCombatAIController* AI = World->SpawnActor<ACLCombatAIController>(
		ACLCombatAIController::StaticClass(), Location, FRotator::ZeroRotator, AIParams);
	if (AI)
	{
		AI->SetBotBookDriven(true);
		AI->Possess(Pawn);
	}
	FCLCharacterAppearance Appear;
	const FString Kit = Def->AbilityStack.Kit.ToString();
	if (Kit.Equals(TEXT("pathfinder"), ESearchCase::IgnoreCase)) Appear.ClassId = ECLClassId::Pathfinder;
	else if (Kit.Equals(TEXT("warden"), ESearchCase::IgnoreCase)) Appear.ClassId = ECLClassId::Warden;
	else Appear.ClassId = ECLClassId::Vanguard;
	if (UCLAbilityLoadoutComponent* Loadout = Pawn->GetAbilities())
	{
		Loadout->LoadFromCharacter(Appear);
		Loadout->ScaleCooldowns(Def->AbilityStack.CooldownScale);
	}
	if (UCLLookController* Look = Pawn->GetLookController())
	{
		Look->SetTrackReactOverride(Def->Intellect.ChangeResponseSeconds);
	}
	UCLParticipantSeat* Seat = NewObject<UCLParticipantSeat>(GetOwner());
	UCLAlgorithmicSeatMotor* Motor = NewObject<UCLAlgorithmicSeatMotor>(Seat);
	Seat->Configure(FGuid::NewGuid(), BotId.ToString(), Motor);
	Seat->SetTeam(ECLPvpTeam::Blue);
	Seat->SetPossession(Pawn->GetPossession());
	Pawn->SetCombatTeam(ECLPvpTeam::Blue);
	Pawn->SetBotDefId(BotId);
	if (Pawn->GetPossession())
	{
		Pawn->GetPossession()->PossessOwn(Pawn);
	}
	NpcSeats.Add(Seat);
	FSpawnedNpc N;
	N.Pawn = Pawn;
	N.Seat = Seat;
	Spawned.Add(N);
	UCLBotBookManager* Books = GI->GetSubsystem<UCLBotBookManager>();
	FString Err;
	if (Books && !Books->AppendCatalog(Seat, Def->Intellect.BotBook.ToString(), Err))
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::Logic, TEXT("raid_botbook_append"), Err));
	}
	return Pawn;
}

void UCLEncounterDirector::FailSpawn(const TCHAR* Code, const FString& Detail)
{
	bRunning = false;
	bWaveInFlight = false;
	UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::Logic, Code, Detail));
	UE_LOG(LogCalling, Error, TEXT("%s %s"), Code, *Detail);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FCLMatchEvent E;
	E.Code = Code;
	E.Detail = Detail;
	E.Time = World->GetTimeSeconds();
	if (ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
	{
		GS->AppendMatchEvent(E);
	}
	if (ACLRaidGameMode* Raid = Cast<ACLRaidGameMode>(GetOwner()))
	{
		Raid->FailBook(FString(Code));
	}
}

void UCLEncounterDirector::SpawnWave()
{
	const FCLWaveHoldEncounter* Enc = WaveHolds.IsValidIndex(EncounterIndex) ? WaveHolds[EncounterIndex] : nullptr;
	if (!Enc || !Enc->Phases.IsValidIndex(PhaseIndex))
	{
		return;
	}
	const FCLWaveHoldPhase& Phase = Enc->Phases[PhaseIndex];
	UWorld* World = GetWorld();
	const ACLTaskMarker* Origin = World ? ACLTaskMarker::FindById(World, Phase.Spawner.OriginMarker) : nullptr;
	const FVector OriginLoc = Origin ? Origin->GetActorLocation() : FVector::ZeroVector;
	const int32 Count = FMath::RandRange(Phase.Spawner.CountMin, FMath::Max(Phase.Spawner.CountMin, Phase.Spawner.CountMax));
	if (Count <= 0)
	{
		FailSpawn(TEXT("raid_spawn_empty"),
			FString::Printf(TEXT("%s %s count=0"), *Enc->Id.ToString(), *Phase.Id.ToString()));
		return;
	}
	int32 SpawnedThis = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		FVector At;
		FString Reason;
		if (!FindClearSpawnLocation(OriginLoc, Phase.Spawner, At, Reason))
		{
			FailSpawn(TEXT("raid_spawn_unclear"),
				FString::Printf(
					TEXT("%s %s origin=%s reason=%s tries=%d radius=%.0f clearR=%.0f placed=%d/%d loc=(%.0f,%.0f,%.0f)"),
					*Enc->Id.ToString(), *Phase.Id.ToString(), *Phase.Spawner.OriginMarker.ToString(),
					*Reason, Phase.Spawner.ClearTries > 0 ? Phase.Spawner.ClearTries : 12,
					Phase.Spawner.RadiusCm, Phase.Spawner.ClearRadiusCm, SpawnedThis, Count,
					OriginLoc.X, OriginLoc.Y, OriginLoc.Z));
			return;
		}
		const FName Bot = RollBot(Phase.Spawner);
		if (!SpawnBot(Bot, At))
		{
			FailSpawn(TEXT("raid_spawn_collide"),
				FString::Printf(
					TEXT("%s %s origin=%s bot=%s placed=%d/%d at=(%.0f,%.0f,%.0f)"),
					*Enc->Id.ToString(), *Phase.Id.ToString(), *Phase.Spawner.OriginMarker.ToString(),
					*Bot.ToString(), SpawnedThis, Count, At.X, At.Y, At.Z));
			return;
		}
		++SpawnedThis;
	}
	++WavesSpawned;
	bWaveInFlight = true;
	WaveTimer = Phase.Spawner.IntervalSeconds;
}

void UCLEncounterDirector::SweepDead()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UCLBotBookManager* Books = GI ? GI->GetSubsystem<UCLBotBookManager>() : nullptr;
	for (int32 i = Spawned.Num() - 1; i >= 0; --i)
	{
		APawn* P = Spawned[i].Pawn.Get();
		ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(P);
		const bool bDead = !P || (Char && !Char->IsCombatAlive());
		if (!bDead)
		{
			continue;
		}
		if (Spawned[i].Seat && Books)
		{
			Books->ClearSeat(Spawned[i].Seat->GetSeatId());
		}
		NpcSeats.Remove(Spawned[i].Seat);
		if (P)
		{
			P->Destroy();
		}
		Spawned.RemoveAt(i);
	}
}

void UCLEncounterDirector::TickNpcs(float DeltaSeconds)
{
	for (FSpawnedNpc& N : Spawned)
	{
		if (N.Seat && N.Seat->GetSeatMotor())
		{
			N.Seat->GetSeatMotor()->TickNet(DeltaSeconds, N.Seat);
		}
	}
}

void UCLEncounterDirector::CompletePhase()
{
	const FCLWaveHoldEncounter* Enc = WaveHolds.IsValidIndex(EncounterIndex) ? WaveHolds[EncounterIndex] : nullptr;
	if (!Enc)
	{
		return;
	}
	if (PhaseIndex + 1 < Enc->Phases.Num())
	{
		StartPhase(PhaseIndex + 1);
		return;
	}
	CompleteEncounter();
}

void UCLEncounterDirector::CompleteEncounter()
{
	ClearSpawned();
	const FCLWaveHoldEncounter* Enc = WaveHolds.IsValidIndex(EncounterIndex) ? WaveHolds[EncounterIndex] : nullptr;
	const FName Opens = Enc ? Enc->OpensMarker : NAME_None;
	if (ACLRaidGameMode* Raid = Cast<ACLRaidGameMode>(GetOwner()))
	{
		Raid->NotifyEncounterComplete(EncounterIndex, Opens);
	}
	if (EncounterIndex + 1 < WaveHolds.Num())
	{
		StartEncounter(EncounterIndex + 1);
		return;
	}
	bRunning = false;
	bFinished = true;
	if (ACLRaidGameMode* Raid = Cast<ACLRaidGameMode>(GetOwner()))
	{
		Raid->AdvanceOrFinishRaid();
	}
}

void UCLEncounterDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bFinished)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (const ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
		{
			if (GS->GetModeResult() != TEXT("in_progress"))
			{
				bRunning = false;
				bAwaitingNav = false;
				return;
			}
		}
	}
	if (bAwaitingNav)
	{
		if (!HasNavTiles())
		{
			return;
		}
		bAwaitingNav = false;
		StartEncounter(0);
	}
	if (!bRunning)
	{
		return;
	}
	TickNpcs(DeltaTime);
	SweepDead();
	const FCLWaveHoldEncounter* Enc = WaveHolds.IsValidIndex(EncounterIndex) ? WaveHolds[EncounterIndex] : nullptr;
	if (!Enc || !Enc->Phases.IsValidIndex(PhaseIndex))
	{
		return;
	}
	const FCLWaveHoldPhase& Phase = Enc->Phases[PhaseIndex];
	if (UWorld* World = GetWorld())
	{
		if (ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
		{
			GS->SetEncounterProgress(Enc->Id, Phase.Id, WavesDone);
		}
	}
	if (Spawned.Num() == 0 && bWaveInFlight)
	{
		bWaveInFlight = false;
			++WavesDone;
			if (WavesSpawned >= Phase.Spawner.Waves)
			{
				CompletePhase();
				return;
			}
			WaveTimer = Phase.Spawner.IntervalSeconds;
	}
	if (!bWaveInFlight && WavesSpawned < Phase.Spawner.Waves)
	{
		WaveTimer -= DeltaTime;
		if (WaveTimer <= 0.f)
		{
			SpawnWave();
		}
	}
}
