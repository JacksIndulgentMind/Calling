#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ability/CLAbilityTypes.h"
#include "Core/CLTunes.h"
#include "CLCombatMovementComponent.generated.h"

/**
 * Combat movement: walk/strafe, sprint, slide (ADS allowed), double jump, dash, mantle stub.
 * Sim step consumes accumulated input from the player controller.
 */
UCLASS()
class CALLING_API UCLCombatMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UCLCombatMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	void SetMoveInput(FVector2D MoveXY);

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	void SetWantsSprint(bool bSprint);

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	void SetWantsCrouch(bool bCrouch);

	/** Ctrl / slide action: rising-edge request evaluated on the next movement tick (after sprint). */
	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	void RequestSlide();

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	void SetIsADS(bool bADS);

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	bool TryStartSlide();

	void EndSlide();
	float GetFullSprintSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	bool TryDash(FVector Direction, float DistanceOverride = -1.f);

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	bool TryDodge();

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	bool TryAirDive();

	void NotifyLanded();
	void SetDivePinGravity(bool bPin);
	/** Per-ring air-steer mul while diving. Negative = use Tune.AirDiveSteer. Does not change AirControl. */
	void SetDiveAirSteer(float Mul);
	float GetDiveElapsed() const { return DiveElapsed; }

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	bool TryMantle();

	UFUNCTION(BlueprintCallable, Category = "Calling|Movement")
	void NotifyJumped();

	void SetJumpStyle(ECLJumpStyle Style) { JumpStyle = Style; }
	void SetAllowSecondJumpFromGround(bool bAllow) { bAllowSecondJumpFromGround = bAllow; }
	void SetHoverSeconds(float Seconds) { HoverSeconds = Seconds; }
	void SetSecondJumpZ(float Z) { SecondJumpZ = Z; }
	FVector GetPendingMoveWorldDir() const;
	FVector GetInputMoveWorldDir() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	bool IsSliding() const { return bSliding; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	bool IsDashing() const { return bDashing; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	bool IsDodging() const { return bDodging; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	bool IsDiving() const { return bDiving; }

	/** True while diving, and for a short window after so /state can observe a fast dive. */
	bool IsDiveReported() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	int32 GetJumpsRemaining() const { return JumpsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDodgeCooldownRemaining() const { return DodgeCooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDodgeCooldownSeconds() const { return Tune.DodgeCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDashDistance() const { return Tune.DashDistance; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDashDuration() const { return Tune.DashDuration; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDodgeDuration() const { return Tune.DodgeDuration; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetSlideDuration() const { return Tune.SlideDuration; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float EstimateSlideTravelCm() const;

	bool CanCommitSlideInDir(const FVector& Dir) const;

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDodgeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetDiveFlipDegrees() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetKneeLandAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	bool IsKneeLanding() const { return KneeLandRemaining > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Calling|Movement")
	float GetCrouchAlpha() const { return CrouchAlpha; }

	void SetMobilityBonus(float Bonus) { MobilityBonus = Bonus; }
	void SetAdsMovePenaltyOverride(float Penalty) { AdsMovePenaltyOverride = Penalty; }

	const FCLMovementTune& GetTune() const { return Tune; }

protected:
	void ReloadSettings();
	void UpdateSpeeds();
	bool TryEnterSlideFromTick();
	void BeginSlide();
	float SlideSpeedAtAlpha(float Alpha) const;
	void UpdateCrouchTransition(float DeltaTime);
	bool HasFloorAt(const FVector& Loc) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Movement")
	FCLMovementTune Tune;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Movement")
	float StandHalfHeight = 96.f;

private:
	FVector2D PendingMove = FVector2D::ZeroVector;
	bool bWantsSprint = false;
	bool bWantsCrouch = false;
	bool bSlideRequested = false;
	bool bWasSprintLastTick = false;
	bool bWasCrouchLastTick = false;
	bool bIsADS = false;
	bool bSliding = false;
	bool bDashing = false;
	bool bDodging = false;
	bool bDiving = false;
	bool bDivePinGravity = false;
	float DiveAirSteerMul = -1.f;
	float SlideTimeRemaining = 0.f;
	float SlideElapsed = 0.f;
	float SlideEntrySpeed = 0.f;
	float SlidePeakSpeed = 0.f;
	float SlideEndSpeed = 0.f;
	FVector SlideDir = FVector::ForwardVector;
	float DashTimeRemaining = 0.f;
	float DodgeTimeRemaining = 0.f;
	float DodgeIFrameRemaining = 0.f;
	float DodgeCooldownRemaining = 0.f;
	float AirDiveCooldownRemaining = 0.f;
	float LastAirDivePulseTime = -100.f;
	float DiveElapsed = 0.f;
	float KneeLandRemaining = 0.f;
	float HoverRemaining = 0.f;
	float CrouchAlpha = 0.f;
	FVector DashVelocity = FVector::ZeroVector;
	int32 JumpsRemaining = 2;
	float MobilityBonus = 0.f;
	float AdsMovePenaltyOverride = -1.f;
	ECLJumpStyle JumpStyle = ECLJumpStyle::RocketPulse;
	bool bAllowSecondJumpFromGround = false;
	float HoverSeconds = 0.f;
	float SecondJumpZ = 560.f;
	void SetDodgeInvulnerable(bool bInvuln);
};
