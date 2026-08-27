#include "Player/CLLookController.h"
#include "Core/CLTunes.h"
#include "Game/CLLobbySubsystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UCLLookController::UCLLookController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCLLookController::ApplyDeltaLook(float DeltaSeconds, FVector2D AccumulatedLook, FVector2D AgentLook)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->Controller || (AccumulatedLook.IsNearlyZero() && AgentLook.IsNearlyZero()))
	{
		return;
	}

	FCLAgentLookTune LookTune;
	LookTune.LoadFromIni();
	const float MaxYaw = LookTune.MaxYawRateDegPerSec * DeltaSeconds;
	const float MaxPitch = LookTune.MaxPitchRateDegPerSec * DeltaSeconds;
	AgentLook.X = FMath::Clamp(AgentLook.X, -MaxYaw, MaxYaw);
	AgentLook.Y = FMath::Clamp(AgentLook.Y, -MaxPitch, MaxPitch);
	Pawn->AddControllerYawInput(AccumulatedLook.X + AgentLook.X);
	Pawn->AddControllerPitchInput(AccumulatedLook.Y + AgentLook.Y);
}

void UCLLookController::NoteHipRecoil()
{
	FCLAgentLookTune RecoilTune;
	RecoilTune.LoadFromIni();
	if (RecoilCorrectRemaining <= 0.f)
	{
		RecoilCorrectRemaining = RecoilTune.RecoilCorrectDelay;
	}
	bRecoilPitchSlow = true;
}

void UCLLookController::ClearAgentLook()
{
	LookGoalYaw.Reset();
	LookGoalPitch.Reset();
	bLookTrack = false;
	LookTrackSeatId.Invalidate();
	bLookStickyValid = false;
	LookSticky = FVector::ZeroVector;
	LookTrackReactRemaining = 0.f;
}

void UCLLookController::SetLookGoalYawPitch(bool bYaw, float Yaw, bool bPitch, float Pitch)
{
	bLookTrack = false;
	LookTrackSeatId.Invalidate();
	bLookStickyValid = false;
	LookTrackReactRemaining = 0.f;
	if (bYaw)
	{
		LookGoalYaw = Yaw;
	}
	else
	{
		LookGoalYaw.Reset();
	}
	if (bPitch)
	{
		LookGoalPitch = Pitch;
	}
	else
	{
		LookGoalPitch.Reset();
	}
}

void UCLLookController::ApplyAgentLookCommand(const FCLLookCommand& Look)
{
	if (!Look.HasAbsolute())
	{
		return;
	}
	const bool bYaw = Look.Mode == ECLLookMode::AbsYaw || Look.Mode == ECLLookMode::AbsBoth;
	const bool bPitch = Look.Mode == ECLLookMode::AbsPitch || Look.Mode == ECLLookMode::AbsBoth;
	SetLookGoalYawPitch(bYaw, Look.Value.X, bPitch, Look.Value.Y);
}

void UCLLookController::SetLookTrackSeat(const FGuid& SeatId)
{
	if (!SeatId.IsValid())
	{
		return;
	}
	LookGoalYaw.Reset();
	LookGoalPitch.Reset();
	if (bLookTrack && LookTrackSeatId == SeatId)
	{
		return;
	}
	bLookTrack = true;
	LookTrackSeatId = SeatId;
	bLookStickyValid = false;
	LookSticky = FVector::ZeroVector;
	LookTrackReactRemaining = 0.f;
}

void UCLLookController::ApplyAgentLookFromStep(const FGuid& TrackSeatId, const FCLLookCommand& Look)
{
	if (TrackSeatId.IsValid())
	{
		SetLookTrackSeat(TrackSeatId);
		return;
	}
	ApplyAgentLookCommand(Look);
}

void UCLLookController::SlewControlToward(const FRotator& Desired, float DeltaSeconds, bool bSlewPitch, float PitchRateDegPerSec)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Ctrl = Pawn ? Pawn->GetController() : nullptr;
	if (!Ctrl)
	{
		return;
	}
	FCLAgentLookTune Tune;
	Tune.LoadFromIni();
	FRotator Cur = Ctrl->GetControlRotation();
	const float YawStep = Tune.MaxYawRateDegPerSec * DeltaSeconds;
	const float YawDelta = FRotator::NormalizeAxis(Desired.Yaw - Cur.Yaw);
	Cur.Yaw += FMath::Clamp(YawDelta, -YawStep, YawStep);
	if (bSlewPitch)
	{
		const float PitchStep = FMath::Max(1.f, PitchRateDegPerSec) * DeltaSeconds;
		const float PitchGoal = FMath::Clamp(FRotator::NormalizeAxis(Desired.Pitch), -89.f, 89.f);
		const float PitchNow = FMath::Clamp(FRotator::NormalizeAxis(Cur.Pitch), -89.f, 89.f);
		Cur.Pitch = FMath::Clamp(PitchNow + FMath::Clamp(PitchGoal - PitchNow, -PitchStep, PitchStep), -89.f, 89.f);
	}
	Ctrl->SetControlRotation(Cur);
}

void UCLLookController::TickAgentLook(float DeltaSeconds)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Ctrl = Pawn ? Pawn->GetController() : nullptr;
	if (!Ctrl)
	{
		return;
	}

	if (RecoilCorrectRemaining > 0.f)
	{
		RecoilCorrectRemaining = FMath::Max(0.f, RecoilCorrectRemaining - DeltaSeconds);
	}

	FRotator Desired = Ctrl->GetControlRotation();
	bool bSlew = false;
	if (bLookTrack && LookTrackSeatId.IsValid())
	{
		AActor* Target = nullptr;
		if (UGameInstance* GI = Pawn->GetGameInstance())
		{
			if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
			{
				Target = Lobby->GetDrivenPawn(LookTrackSeatId);
			}
		}
		if (Target)
		{
			FCLAgentLookTune Tune;
			Tune.LoadFromIni();
			const FVector Loc = Target->GetActorLocation();
			if (!bLookStickyValid)
			{
				LookSticky = Loc;
				bLookStickyValid = true;
				LookTrackReactRemaining = 0.f;
			}
			else if (FVector::Dist(Loc, LookSticky) > Tune.TrackMoveEpsilonCm && LookTrackReactRemaining <= 0.f)
			{
				LookTrackReactRemaining = Tune.TrackReactSeconds;
			}
			if (LookTrackReactRemaining > 0.f)
			{
				LookTrackReactRemaining -= DeltaSeconds;
				if (LookTrackReactRemaining <= 0.f)
				{
					LookSticky = Loc;
					LookTrackReactRemaining = 0.f;
				}
			}
		}
		if (bLookStickyValid)
		{
			const FVector To = LookSticky - Pawn->GetActorLocation();
			if (!To.IsNearlyZero())
			{
				Desired = To.Rotation();
				bSlew = true;
			}
		}
	}
	else if (LookGoalYaw.IsSet() || LookGoalPitch.IsSet())
	{
		if (LookGoalYaw.IsSet())
		{
			Desired.Yaw = LookGoalYaw.GetValue();
		}
		if (LookGoalPitch.IsSet())
		{
			Desired.Pitch = LookGoalPitch.GetValue();
		}
		bSlew = true;
	}

	if (bSlew)
	{
		FCLAgentLookTune Tune;
		Tune.LoadFromIni();
		const bool bSlewPitch = RecoilCorrectRemaining <= 0.f;
		float PitchRate = Tune.MaxPitchRateDegPerSec;
		if (bSlewPitch && bRecoilPitchSlow)
		{
			PitchRate = Tune.RecoilCorrectPitchRate;
			const float PitchErr = FMath::Abs(FRotator::NormalizeAxis(Desired.Pitch) - FRotator::NormalizeAxis(Ctrl->GetControlRotation().Pitch));
			if (PitchErr < 1.f)
			{
				bRecoilPitchSlow = false;
			}
		}
		SlewControlToward(Desired, DeltaSeconds, bSlewPitch, PitchRate);
	}
}
