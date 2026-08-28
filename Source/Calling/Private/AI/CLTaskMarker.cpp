#include "AI/CLTaskMarker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SceneComponent.h"

ACLTaskMarker::ACLTaskMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetCanEverAffectNavigation(false);
	SetRootComponent(Root);
	SetActorEnableCollision(false);
	bNetLoadOnClient = false;
}

ACLTaskMarker* ACLTaskMarker::SpawnAt(UWorld* World, FName MarkerId, const FVector& Location, float ZoneRadiusCm)
{
	if (!World || MarkerId.IsNone())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACLTaskMarker* Marker = World->SpawnActor<ACLTaskMarker>(Location, FRotator::ZeroRotator, Params);
	if (Marker)
	{
		Marker->Id = MarkerId;
		Marker->ZoneRadiusCm = ZoneRadiusCm;
	}
	return Marker;
}

ACLTaskMarker* ACLTaskMarker::FindById(UWorld* World, FName MarkerId)
{
	if (!World || MarkerId.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<ACLTaskMarker> It(World); It; ++It)
	{
		if (It->Id == MarkerId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ACLTaskMarker::DestroyAllInWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}
	TArray<ACLTaskMarker*> Kill;
	for (TActorIterator<ACLTaskMarker> It(World); It; ++It)
	{
		Kill.Add(*It);
	}
	for (ACLTaskMarker* Marker : Kill)
	{
		if (Marker)
		{
			Marker->Destroy();
		}
	}
}
