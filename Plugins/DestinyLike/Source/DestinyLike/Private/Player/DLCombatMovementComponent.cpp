#include "Player/DLCombatMovementComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Player/DLPlayerCharacter.h"
#include "Nav/DLNavTune.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Misc/ConfigCacheIni.h"

UDLCombatMovementComponent::UDLCombatMovementComponent()
{
	NavAgentProps.bCanCrouch = true;
	bCanWalkOffLedges = true;
	bCanWalkOffLedgesWhenCrouching = true;
	MaxStepHeight = 70.f;
	MaxWalkSpeed = 420.f;
	MaxWalkSpeedCrouched = 210.f;
	JumpZVelocity = 640.f;
	AirControl = 0.35f;
	GroundFriction = 8.f;
	BrakingDecelerationWalking = 2048.f;
}

void UDLCombatMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ReloadSettings();
	JumpsRemaining = Tune.MaxJumps;
	UpdateSpeeds();
}

void UDLCombatMovementComponent::ReloadSettings()
{
	Tune.LoadFromIni();
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("JumpZVelocity"), JumpZVelocity, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLMovementFeelSettings"), TEXT("DoubleJumpZVelocity"), SecondJumpZ, GGameIni);
	CrouchedHalfHeight = Tune.CrouchHalfHeight;
	MaxStepHeight = DLNavTune::Get().MaxStepHeightCm;
}

void UDLCombatMovementComponent::SetMoveInput(FVector2D MoveXY)
{
	PendingMove = MoveXY;
}

void UDLCombatMovementComponent::SetWantsSprint(bool bSprint) { bWantsSprint = bSprint; }
void UDLCombatMovementComponent::SetWantsCrouch(bool bCrouch) { bWantsCrouch = bCrouch; }
void UDLCombatMovementComponent::RequestSlide() { bSlideRequested = true; }
void UDLCombatMovementComponent::SetIsADS(bool bADS) { bIsADS = bADS; }

float UDLCombatMovementComponent::GetFullSprintSpeed() const
{
	return Tune.BaseWalkSpeed * (1.f + MobilityBonus) * Tune.SprintSpeedMultiplier;
}

FVector UDLCombatMovementComponent::GetInputMoveWorldDir() const
{
	if (const ACharacter* Char = GetCharacterOwner())
	{
		const FRotator Yaw(0.f, Char->GetControlRotation().Yaw, 0.f);
		const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
		return (Forward * PendingMove.Y + Right * PendingMove.X).GetSafeNormal2D();
	}
	return FVector::ZeroVector;
}

FVector UDLCombatMovementComponent::GetPendingMoveWorldDir() const
{
	const FVector Input = GetInputMoveWorldDir();
	if (!Input.IsNearlyZero())
	{
		return Input;
	}
	if (const ACharacter* Char = GetCharacterOwner())
	{
		return Char->GetActorForwardVector().GetSafeNormal2D();
	}
	return FVector::ForwardVector;
}

void UDLCombatMovementComponent::SetDodgeInvulnerable(bool bInvuln)
{
	if (AActor* Owner = GetOwner())
	{
		if (UDLHealthShieldComponent* HS = Owner->FindComponentByClass<UDLHealthShieldComponent>())
		{
			HS->SetInvulnerable(bInvuln);
		}
	}
}

void UDLCombatMovementComponent::NotifyJumped()
{
	if (bDiving)
	{
		return;
	}
	EndSlide();
	if (IsMovingOnGround())
	{
		JumpsRemaining = Tune.MaxJumps - 1;
		if (JumpStyle == EDLJumpStyle::InertiaDamp)
		{
			HoverRemaining = HoverSeconds;
			Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity * 1.15f);
		}
	}
	else if (JumpsRemaining > 0)
	{
		if (JumpStyle == EDLJumpStyle::RocketPulse)
		{
			--JumpsRemaining;
			const float PulseZ = SecondJumpZ > 0.f ? SecondJumpZ : JumpZVelocity;
			Velocity.Z = FMath::Max(Velocity.Z, 0.f) + PulseZ;
		}
		else if (bAllowSecondJumpFromGround)
		{
			--JumpsRemaining;
			Velocity.Z = SecondJumpZ > 0.f ? SecondJumpZ : JumpZVelocity * 0.4f;
		}
	}
}

float UDLCombatMovementComponent::EstimateSlideTravelCm() const
{
	const float Sprint = GetFullSprintSpeed();
	const float Entry = bSliding ? SlideEntrySpeed : Sprint;
	const float Peak = FMath::Max(Sprint * Tune.SlidePeakMultiplier, Entry);
	const float End = Sprint * Tune.SlideEndMultiplier;
	const float Dur = FMath::Max(0.05f, Tune.SlideDuration);
	const float AccelEnd = FMath::Clamp(Tune.SlideAccelPortion, 0.05f, 0.95f);
	const float T1 = Dur * AccelEnd;
	const float T2 = Dur - T1;
	return 0.5f * (Entry + Peak) * T1 + 0.5f * (Peak + End) * T2;
}

bool UDLCombatMovementComponent::HasFloorAt(const FVector& Loc) const
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DLSlideFloor), false, Owner);
	const FVector Start = Loc + FVector(0.f, 0.f, 90.f);
	const FVector End = Loc + FVector(0.f, 0.f, -280.f);
	return World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
}

bool UDLCombatMovementComponent::CanCommitSlideInDir(const FVector& Dir) const
{
	const FVector SafeDir = Dir.GetSafeNormal2D();
	if (SafeDir.IsNearlyZero() || !GetOwner())
	{
		return false;
	}
	const float Travel = EstimateSlideTravelCm();
	const FVector Start = GetOwner()->GetActorLocation();
	for (int32 i = 1; i <= 3; ++i)
	{
		const float T = static_cast<float>(i) / 3.f;
		if (!HasFloorAt(Start + SafeDir * (Travel * T)))
		{
			return false;
		}
	}
	return true;
}

float UDLCombatMovementComponent::GetDodgeAlpha() const
{
	if (!bDodging || Tune.DodgeDuration <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - DodgeTimeRemaining / Tune.DodgeDuration, 0.f, 1.f);
}

float UDLCombatMovementComponent::GetDiveFlipDegrees() const
{
	if (!bDiving)
	{
		return 0.f;
	}
	return 360.f * FMath::Clamp(DiveElapsed / 0.45f, 0.f, 1.f);
}

float UDLCombatMovementComponent::GetKneeLandAlpha() const
{
	return FMath::Clamp(KneeLandRemaining / 0.35f, 0.f, 1.f);
}

void UDLCombatMovementComponent::EndSlide()
{
	if (!bSliding)
	{
		return;
	}
	bSliding = false;
	SlideTimeRemaining = 0.f;
	GroundFriction = 8.f;
	BrakingDecelerationWalking = 2048.f;
}

void UDLCombatMovementComponent::BeginSlide()
{
	bSliding = true;
	SlideElapsed = 0.f;
	SlideTimeRemaining = Tune.SlideDuration;
	const FVector MoveDir = GetPendingMoveWorldDir();
	const FVector VelDir = Velocity.GetSafeNormal2D();
	SlideDir = !MoveDir.IsNearlyZero() ? MoveDir : VelDir;
	if (SlideDir.IsNearlyZero())
	{
		SlideDir = FVector::ForwardVector;
	}

	const float Sprint = GetFullSprintSpeed();
	SlideEntrySpeed = Velocity.Size2D();
	SlidePeakSpeed = FMath::Max(Sprint * Tune.SlidePeakMultiplier, SlideEntrySpeed);
	SlideEndSpeed = Sprint * Tune.SlideEndMultiplier;
	GroundFriction = 0.f;
	BrakingDecelerationWalking = 0.f;
}

float UDLCombatMovementComponent::SlideSpeedAtAlpha(float Alpha) const
{
	const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
	const float AccelEnd = FMath::Clamp(Tune.SlideAccelPortion, 0.05f, 0.95f);
	if (Clamped <= AccelEnd)
	{
		return FMath::Lerp(SlideEntrySpeed, SlidePeakSpeed, Clamped / AccelEnd);
	}
	return FMath::Lerp(SlidePeakSpeed, SlideEndSpeed, (Clamped - AccelEnd) / (1.f - AccelEnd));
}

bool UDLCombatMovementComponent::TryEnterSlideFromTick()
{
	if (!IsMovingOnGround() || bSliding || bDodging || bDashing)
	{
		bSlideRequested = false;
		return false;
	}

	const bool bCrouchHeld = bWantsCrouch || bSlideRequested;
	const bool bCrouchEdge = (bCrouchHeld && !bWasCrouchLastTick) || bSlideRequested;
	bSlideRequested = false;
	if (!bCrouchEdge)
	{
		return false;
	}

	const bool bSprintLatch = bWantsSprint || bWasSprintLastTick;
	const bool bSprintSpeedFallback = Velocity.Size2D() >= GetFullSprintSpeed() - 1.f;
	if (!bSprintLatch && !bSprintSpeedFallback)
	{
		return false;
	}

	FVector CommitDir = GetPendingMoveWorldDir();
	if (CommitDir.IsNearlyZero())
	{
		CommitDir = Velocity.GetSafeNormal2D();
	}
	if (!CanCommitSlideInDir(CommitDir))
	{
		return false;
	}

	BeginSlide();
	return true;
}

bool UDLCombatMovementComponent::TryStartSlide()
{
	RequestSlide();
	return TryEnterSlideFromTick();
}

bool UDLCombatMovementComponent::TryDash(FVector Direction, float DistanceOverride)
{
	if (bSliding)
	{
		if (!Tune.bAllowSlideDashCancel)
		{
			return false;
		}
		EndSlide();
	}
	if (bDashing || bDodging)
	{
		return false;
	}

	FVector Dir = Direction.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		if (const ACharacter* Char = GetCharacterOwner())
		{
			Dir = Char->GetActorForwardVector().GetSafeNormal2D();
		}
	}
	if (Dir.IsNearlyZero())
	{
		return false;
	}

	const float Dist = DistanceOverride > 0.f ? DistanceOverride : Tune.DashDistance;
	bDashing = true;
	DashTimeRemaining = FMath::Max(0.08f, Tune.DashDuration);
	DashVelocity = Dir * (Dist / DashTimeRemaining);
	if (Tune.DashHopZ > 0.f)
	{
		Velocity.Z = FMath::Max(Velocity.Z, Tune.DashHopZ);
		if (MovementMode != MOVE_Falling)
		{
			SetMovementMode(MOVE_Falling);
		}
	}
	return true;
}

bool UDLCombatMovementComponent::TryDodge()
{
	if (bSliding)
	{
		if (!Tune.bAllowSlideDodgeCancel)
		{
			return false;
		}
		EndSlide();
	}
	if (DodgeCooldownRemaining > 0.f || bDodging || bDashing || !IsMovingOnGround())
	{
		return false;
	}

	const ACharacter* Char = GetCharacterOwner();
	const FVector Forward = Char ? Char->GetActorForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
	FVector Dir = GetInputMoveWorldDir();
	if (Dir.IsNearlyZero())
	{
		Dir = -Forward;
	}

	float Dist = Tune.DodgeDistance;
	const float Forwardness = FVector::DotProduct(Dir, Forward);
	if (Forwardness > 0.2f)
	{
		Dist *= FMath::Lerp(1.f, FMath::Clamp(Tune.DodgeForwardScale, 0.4f, 1.f), Forwardness);
	}

	bDodging = true;
	DodgeTimeRemaining = Tune.DodgeDuration;
	DodgeIFrameRemaining = Tune.DodgeIFrames;
	DashVelocity = Dir * (Dist / FMath::Max(0.05f, Tune.DodgeDuration));
	DodgeCooldownRemaining = Tune.DodgeCooldownSeconds;
	SetDodgeInvulnerable(true);
	return true;
}

bool UDLCombatMovementComponent::TryAirDive()
{
	if (IsMovingOnGround())
	{
		return false;
	}
	if (bDiving || AirDiveCooldownRemaining > 0.f)
	{
		return false;
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now - LastAirDivePulseTime < Tune.AirDiveCoalesceSeconds)
	{
		return false;
	}
	LastAirDivePulseTime = Now;

	EndSlide();
	bDiving = true;
	DiveElapsed = 0.f;
	JumpsRemaining = 0;
	AirDiveCooldownRemaining = FMath::Max(0.05f, Tune.AirDiveCooldownSeconds);
	const float Keep = FMath::Clamp(Tune.AirDiveXYKeep, 0.f, 1.f);
	Velocity.X *= Keep;
	Velocity.Y *= Keep;
	GravityScale = Tune.DiveHangGravity;
	if (MovementMode != MOVE_Falling)
	{
		SetMovementMode(MOVE_Falling);
	}
	return true;
}

void UDLCombatMovementComponent::NotifyLanded()
{
	if (bDiving)
	{
		KneeLandRemaining = 0.35f;
	}
	bDiving = false;
}

bool UDLCombatMovementComponent::IsDiveReported() const
{
	if (bDiving)
	{
		return true;
	}
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastAirDivePulseTime) < 0.35f;
}

bool UDLCombatMovementComponent::TryMantle()
{
	// Stub: project can replace with climb trace + montage.
	if (!IsMovingOnGround() || !GetOwner())
	{
		return false;
	}

	FHitResult Hit;
	const FVector Start = GetOwner()->GetActorLocation() + FVector(0, 0, 40.f);
	const FVector End = Start + GetOwner()->GetActorForwardVector() * Tune.MantleReachDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DLMantle), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		const FVector MantleTo = Hit.ImpactPoint + Hit.ImpactNormal * -40.f + FVector(0, 0, 90.f);
		GetOwner()->SetActorLocation(MantleTo, false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}
	return false;
}

void UDLCombatMovementComponent::UpdateSpeeds()
{
	float Speed = Tune.BaseWalkSpeed * (1.f + MobilityBonus);
	if (bWantsSprint && !bIsADS)
	{
		Speed *= Tune.SprintSpeedMultiplier;
	}

	const float Penalty = AdsMovePenaltyOverride >= 0.f ? AdsMovePenaltyOverride : Tune.ADSMovePenalty;
	if (bIsADS && !(bSliding && Tune.bAllowADSWhileSliding))
	{
		Speed *= (1.f - Penalty);
	}
	else if (bIsADS && bSliding && Tune.bAllowADSWhileSliding)
	{
		// Milder penalty while sliding + ADS (muscle-memory friendly).
		Speed *= (1.f - Penalty * 0.5f);
	}

	if (bSliding)
	{
		const float Alpha = Tune.SlideDuration > 0.f ? (1.f - SlideTimeRemaining / Tune.SlideDuration) : 1.f;
		Speed = SlideSpeedAtAlpha(Alpha);
	}

	if (bDodging || bDashing)
	{
		Speed = FMath::Max(Speed, DashVelocity.Size2D());
	}

	MaxWalkSpeed = Speed;
	MaxWalkSpeedCrouched = (bSliding || bDodging || bDashing) ? Speed : Speed * 0.5f;
}

void UDLCombatMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (IsMovingOnGround())
	{
		JumpsRemaining = Tune.MaxJumps;
		HoverRemaining = 0.f;
		GravityScale = 1.f;
	}

	if (DodgeCooldownRemaining > 0.f)
	{
		DodgeCooldownRemaining -= DeltaTime;
	}
	if (AirDiveCooldownRemaining > 0.f)
	{
		AirDiveCooldownRemaining -= DeltaTime;
	}

	if (KneeLandRemaining > 0.f)
	{
		KneeLandRemaining = FMath::Max(0.f, KneeLandRemaining - DeltaTime);
	}

	if (bDiving)
	{
		if (IsMovingOnGround())
		{
			KneeLandRemaining = 0.35f;
			bDiving = false;
			GravityScale = 1.f;
		}
		else
		{
			DiveElapsed += DeltaTime;
			HoverRemaining = 0.f;
			if (DiveElapsed < Tune.DiveHangSeconds)
			{
				GravityScale = Tune.DiveHangGravity;
			}
			else
			{
				GravityScale = Tune.DiveFallGravity;
				if (Velocity.Z < Tune.AirDiveDownSpeed)
				{
					Velocity.Z = Tune.AirDiveDownSpeed;
				}
			}
			const ACharacter* DiveChar = GetCharacterOwner();
			const bool bDiveSteer = DiveChar && PendingMove.SizeSquared() > 0.0225f;
			if (bDiveSteer)
			{
				const FRotator Yaw(0.f, DiveChar->GetControlRotation().Yaw, 0.f);
				const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
				const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
				FVector Wish = Forward * PendingMove.Y + Right * PendingMove.X;
				Wish.Z = 0.f;
				const float WishSize = Wish.Size();
				if (WishSize > 1.f)
				{
					Wish /= WishSize;
				}
				const float Accel = GetMaxAcceleration() * FMath::Clamp(Tune.AirDiveSteer, 0.f, 6.f);
				Velocity.X += Wish.X * Accel * DeltaTime;
				Velocity.Y += Wish.Y * Accel * DeltaTime;
				const float MaxXY = FMath::Max(GetFullSprintSpeed(), Tune.AirDiveMaxXY);
				const float XY = Velocity.Size2D();
				if (XY > MaxXY && MaxXY > 1.f)
				{
					const float Scale = MaxXY / XY;
					Velocity.X *= Scale;
					Velocity.Y *= Scale;
				}
			}
			else
			{
				const float XY = Velocity.Size2D();
				if (XY > 1.f)
				{
					const float Brake = GetMaxAcceleration() * FMath::Clamp(Tune.AirDiveXYBrake, 0.f, 8.f);
					const float Drop = FMath::Min(XY, Brake * DeltaTime);
					const float Scale = (XY - Drop) / XY;
					Velocity.X *= Scale;
					Velocity.Y *= Scale;
				}
				Acceleration.X = 0.f;
				Acceleration.Y = 0.f;
			}
			if (MovementMode != MOVE_Falling)
			{
				SetMovementMode(MOVE_Falling);
			}
		}
	}

	if (HoverRemaining > 0.f && !IsMovingOnGround() && !bDiving)
	{
		HoverRemaining -= DeltaTime;
		GravityScale = 0.18f;
		if (Velocity.Z < 0.f)
		{
			Velocity.Z *= 0.45f;
		}
	}
	else if (!bDiving && (JumpStyle != EDLJumpStyle::InertiaDamp || HoverRemaining <= 0.f))
	{
		GravityScale = 1.f;
	}

	if (bDashing)
	{
		DashTimeRemaining -= DeltaTime;
		Velocity = FVector(DashVelocity.X, DashVelocity.Y, Velocity.Z);
		if (DashTimeRemaining <= 0.f)
		{
			bDashing = false;
		}
	}
	else if (bDodging)
	{
		DodgeTimeRemaining -= DeltaTime;
		DodgeIFrameRemaining -= DeltaTime;
		Velocity = FVector(DashVelocity.X, DashVelocity.Y, Velocity.Z);
		if (DodgeIFrameRemaining <= 0.f)
		{
			SetDodgeInvulnerable(false);
		}
		if (DodgeTimeRemaining <= 0.f)
		{
			SetDodgeInvulnerable(false);
			bDodging = false;
		}
	}

	if (!bSliding)
	{
		TryEnterSlideFromTick();
	}

	if (bSliding)
	{
		SlideElapsed += DeltaTime;
		SlideTimeRemaining -= DeltaTime;
		const float Alpha = Tune.SlideDuration > 0.f ? 1.f - (SlideTimeRemaining / Tune.SlideDuration) : 1.f;
		const float Speed = SlideSpeedAtAlpha(Alpha);
		Velocity = FVector(SlideDir.X * Speed, SlideDir.Y * Speed, Velocity.Z);
		if (SlideTimeRemaining <= 0.f)
		{
			EndSlide();
		}
	}

	if (ACharacter* Char = GetCharacterOwner())
	{
		UpdateCrouchTransition(DeltaTime);

		if (!bSliding && !bDodging && !bDashing && !bDiving)
		{
			const FRotator Yaw(0.f, Char->GetControlRotation().Yaw, 0.f);
			const FVector Forward = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
			const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
			Char->AddMovementInput(Forward, PendingMove.Y);
			Char->AddMovementInput(Right, PendingMove.X);
		}
	}

	bWasSprintLastTick = bWantsSprint;
	bWasCrouchLastTick = bWantsCrouch || bSliding;

	UpdateSpeeds();
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bSliding && !IsMovingOnGround())
	{
		if (MovementMode != MOVE_Falling)
		{
			SetMovementMode(MOVE_Falling);
		}
		const float Alpha = Tune.SlideDuration > 0.f ? 1.f - (SlideTimeRemaining / Tune.SlideDuration) : 1.f;
		const float Speed = SlideSpeedAtAlpha(Alpha);
		Velocity = FVector(SlideDir.X * Speed, SlideDir.Y * Speed, Velocity.Z);
	}

	if (bDodging || bDashing)
	{
		Velocity = FVector(DashVelocity.X, DashVelocity.Y, Velocity.Z);
	}
}

void UDLCombatMovementComponent::UpdateCrouchTransition(float DeltaTime)
{
	ACharacter* Char = GetCharacterOwner();
	if (!Char)
	{
		return;
	}

	const bool bWantLow = bWantsCrouch || bSliding;
	const float Rate = 1.f / FMath::Max(0.05f, Tune.CrouchTransitionSeconds);
	CrouchAlpha = FMath::Clamp(CrouchAlpha + (bWantLow ? Rate : -Rate) * DeltaTime, 0.f, 1.f);

	const float Half = FMath::Lerp(StandHalfHeight, Tune.CrouchHalfHeight, CrouchAlpha);
	if (UCapsuleComponent* Cap = Char->GetCapsuleComponent())
	{
		if (!FMath::IsNearlyEqual(Cap->GetUnscaledCapsuleHalfHeight(), Half, 0.1f))
		{
			Cap->SetCapsuleHalfHeight(Half, true);
		}
	}

	Char->bIsCrouched = CrouchAlpha >= 0.5f;

	if (const ADLPlayerCharacter* DLChar = Cast<ADLPlayerCharacter>(Char))
	{
		if (UStaticMeshComponent* Body = DLChar->GetBodyMesh())
		{
			FVector Loc = Body->GetRelativeLocation();
			Loc.Z = -Half;
			Body->SetRelativeLocation(Loc);
		}
	}
}
