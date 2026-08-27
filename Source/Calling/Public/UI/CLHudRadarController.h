#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CLHudRadarController.generated.h"

class ACLPlayerCharacter;

struct FCLRadarTrack
{
	TWeakObjectPtr<AActor> Actor;
	float Alpha = 0.f;
	float ScatterYawDeg = 0.f;
	float ScatterRadial = 0.f;
	FVector2D LastOffset = FVector2D::ZeroVector;
	bool bHasOffset = false;
	TArray<FVector2D> Trail;
};

struct FCLRadarPaintBlip
{
	FVector2D Offset = FVector2D::ZeroVector;
	TArray<FVector2D> Trail;
	FLinearColor Color = FLinearColor::White;
	float Alpha = 0.f;
	float Size = 3.f;
};

/** Radar contact tracks and paint blips. */
UCLASS()
class CALLING_API UCLHudRadarController : public UObject
{
	GENERATED_BODY()

public:
	void Refresh(const ACLPlayerCharacter* Viewer, float DeltaTime);
	const TArray<FCLRadarPaintBlip>& GetBlips() const { return RadarBlips; }
	const float* GetWedge() const { return RadarWedge; }

protected:
	TArray<FCLRadarTrack> RadarTracks;
	TArray<FCLRadarPaintBlip> RadarBlips;
	float RadarScatterClock = 0.f;
	float RadarWedge[3] = {};
};
