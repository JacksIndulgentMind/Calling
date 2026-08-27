#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ability/DLAbilityTypes.h"
#include "Core/DLTunes.h"
#include "DLCombatMovementComponent.generated.h"

/**
 * Combat movement: walk/strafe, sprint, slide (ADS allowed), double jump, dash, mantle stub.
 * Sim step consumes accumulated input from the player controller.
 */
UCLASS()
class DESTINYLIKE_API UDLCombatMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDLCombatMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	void SetMoveInput(FVector2D MoveXY);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	void SetWantsSprint(bool bSprint);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	void SetWantsCrouch(bool bCrouch);

	/** Ctrl / slide action: rising-edge request evaluated on the next movement tick (after sprint). */
	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	void RequestSlide();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	void SetIsADS(bool bADS);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	bool TryStartSlide();

	void EndSlide();
	float GetFullSprintSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	bool TryDash(FVector Direction, float DistanceOverride = -1.f);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	bool TryDodge();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	bool TryAirDive();

	void NotifyLanded();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	bool TryMantle();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Movement")
	void NotifyJumped();

	void SetJumpStyle(EDLJumpStyle Style) { JumpStyle = Style; }
	void SetAllowSecondJumpFromGround(bool bAllow) { bAllowSecondJumpFromGround = bAllow; }
	void SetHoverSeconds(float Seconds) { HoverSeconds = Seconds; }
	void SetSecondJumpZ(float Z) { SecondJumpZ = Z; }
	FVector GetPendingMoveWorldDir() const;
	FVector GetInputMoveWorldDir() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	bool IsSliding() const { return bSliding; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	bool IsDashing() const { return bDashing; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	bool IsDodging() const { return bDodging; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	bool IsDiving() const { return bDiving; }

	/** True while diving, and for a short window after so /state can observe a fast dive. */
	bool IsDiveReported() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	int32 GetJumpsRemaining() const { return JumpsRemaining; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDodgeCooldownRemaining() const { return DodgeCooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDodgeCooldownSeconds() const { return Tune.DodgeCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDashDistance() const { return Tune.DashDistance; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDashDuration() const { return Tune.DashDuration; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDodgeDuration() const { return Tune.DodgeDuration; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetSlideDuration() const { return Tune.SlideDuration; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float EstimateSlideTravelCm() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDodgeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetDiveFlipDegrees() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetKneeLandAlpha() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	bool IsKneeLanding() const { return KneeLandRemaining > 0.f; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Movement")
	float GetCrouchAlpha() const { return CrouchAlpha; }

	void SetMobilityBonus(float Bonus) { MobilityBonus = Bonus; }
	void SetAdsMovePenaltyOverride(float Penalty) { AdsMovePenaltyOverride = Penalty; }

	const FDLMovementTune& GetTune() const { return Tune; }

protected:
	void ReloadSettings();
	void UpdateSpeeds();
	bool TryEnterSlideFromTick();
	void BeginSlide();
	float SlideSpeedAtAlpha(float Alpha) const;
	void UpdateCrouchTransition(float DeltaTime);
	bool CanCommitSlideInDir(const FVector& Dir) const;
	bool HasFloorAt(const FVector& Loc) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Movement")
	FDLMovementTune Tune;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Movement")
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
	EDLJumpStyle JumpStyle = EDLJumpStyle::RocketPulse;
	bool bAllowSecondJumpFromGround = false;
	float HoverSeconds = 0.f;
	float SecondJumpZ = 560.f;
	void SetDodgeInvulnerable(bool bInvuln);
};
