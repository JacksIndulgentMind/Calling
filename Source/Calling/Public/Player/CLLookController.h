#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/CLAgentIntent.h"
#include "Misc/Optional.h"
#include "CLLookController.generated.h"

/** Absolute look goals, seat tracking, and recoil-aware slew. */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLLookController : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLLookController();

	void ApplyDeltaLook(float DeltaSeconds, FVector2D AccumulatedLook, FVector2D AgentLook);
	void TickAgentLook(float DeltaSeconds);
	void ApplyAgentLookCommand(const FCLLookCommand& Look);
	void ApplyAgentLookFromStep(const FGuid& TrackSeatId, const FCLLookCommand& Look);
	void SetLookTrackSeat(const FGuid& SeatId);
	void SetTrackReactOverride(float Seconds);
	float GetTrackReactSeconds() const;
	FGuid GetLookTrackSeat() const { return LookTrackSeatId; }
	bool IsLookTracking() const { return bLookTrack; }
	void ClearAgentLook();
	void SetLookGoalYawPitch(bool bYaw, float Yaw, bool bPitch, float Pitch);
	void NoteHipRecoil();

protected:
	void SlewControlToward(const FRotator& Desired, float DeltaSeconds, bool bSlewPitch, float PitchRateDegPerSec);

	TOptional<float> LookGoalYaw;
	TOptional<float> LookGoalPitch;
	bool bLookTrack = false;
	FGuid LookTrackSeatId;
	float TrackReactOverride = -1.f;
	bool bLookStickyValid = false;
	FVector LookSticky = FVector::ZeroVector;
	float LookTrackReactRemaining = 0.f;
	float RecoilCorrectRemaining = 0.f;
	bool bRecoilPitchSlow = false;
};
