#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/DLTypes.h"
#include "Input/DLAgentIntent.h"
#include "TimerManager.h"
#include "DLPlayerCharacter.generated.h"

class UDLCombatMovementComponent;
class UDLHealthShieldComponent;
class UDLWeaponMotorComponent;
class UDLWeaponBehaviorComponent;
class UDLAbilityLoadoutComponent;
class UDLPossessionComponent;
class UDLDamageableComponent;
class UDLEffectStackComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class DESTINYLIKE_API ADLPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADLPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike")
	void ApplyProfileLoadout();

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLCombatMovementComponent* GetCombatMovement() const { return CombatMovement; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLHealthShieldComponent* GetHealthShield() const { return HealthShield; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLDamageableComponent* GetDamageable() const { return Damageable; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLEffectStackComponent* GetEffectStack() const { return EffectStack; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLWeaponMotorComponent* GetWeaponMotor() const { return WeaponMotor; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLAbilityLoadoutComponent* GetAbilities() const { return Abilities; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UDLPossessionComponent* GetPossession() const { return Possession; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	bool IsSliding() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }

	USceneComponent* GetViewWeaponRoot() const { return ViewWeaponRoot; }
	USceneComponent* GetWorldWeaponRoot() const { return WorldWeaponRoot; }
	bool UsesViewWeapon() const;
	float GetThirdPersonAlpha() const { return ThirdPersonAlpha; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	virtual void SetDemoViewActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "DestinyLike")
	float GetHurtAlpha() const;

	FVector2D GetAdsReticlePunch() const;

	void SetRippleCamo(bool bEnabled);

	/** Shared intent seam: HTTP, /goto, and in-game playbooks. Holdables latch; look/pulses consume. */
	void ApplyAgentIntent(const FDLAgentIntent& Intent);

	void ApplyAgentIntent(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire,
		bool bJump, bool bDodge, bool bDash, bool bReload, bool bSwap);

	/** Zero stick and pending pulses. Sequence / goto / empty intent call this. */
	void ClearAgentIntent();

	void ApplyAgentLookCommand(const FDLLookCommand& Look);
	void ApplyAgentLookFromStep(const FGuid& TrackSeatId, const FDLLookCommand& Look);
	void SetLookTrackSeat(const FGuid& SeatId);
	void ClearAgentLook();
	void SetLookGoalYawPitch(bool bYaw, float Yaw, bool bPitch, float Pitch);

	/** Fixed-sim consumer: apply accumulated look/move from controller. */
	void ConsumeSimInput(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire);

	bool IsCombatAlive() const;
	void NoteIncomingDamage(AController* InstigatorController, float Applied);
	void PlayKnifeSlash();
	void NotifyRespawned();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLCombatMovementComponent> CombatMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLHealthShieldComponent> HealthShield;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLDamageableComponent> Damageable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLEffectStackComponent> EffectStack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLWeaponBehaviorComponent> WeaponBehavior;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLWeaponMotorComponent> WeaponMotor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLAbilityLoadoutComponent> Abilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UDLPossessionComponent> Possession;

	UPROPERTY()
	bool bUseNpcLoadout = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DestinyLike")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY()
	TObjectPtr<USceneComponent> ViewWeaponRoot;

	UPROPERTY()
	TObjectPtr<USceneComponent> WorldWeaponRoot;

	FDelegateHandle FixedTickHandle;
	void HandleFixedGameTick(float DeltaSeconds);

	UPROPERTY()
	FVector2D AccumulatedMove = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D AccumulatedLook = FVector2D::ZeroVector;

	bool bAccumSprint = false;
	bool bAccumCrouch = false;
	bool bAccumADS = false;
	bool bAccumFire = false;

	FVector2D AgentMove = FVector2D::ZeroVector;
	FVector2D AgentLook = FVector2D::ZeroVector;
	bool bAgentSprint = false;
	bool bAgentCrouch = false;
	bool bAgentADS = false;
	bool bAgentFire = false;
	bool bAgentJump = false;
	bool bAgentDodge = false;
	bool bAgentDash = false;
	bool bAgentReload = false;
	bool bAgentSwap = false;
	bool bAgentSlide = false;
	bool bAgentAirDive = false;
	bool bAgentMelee = false;
	bool bAgentWeaponPrimary = false;
	bool bAgentWeaponSpecial = false;
	FName AgentSightId = NAME_None;

	bool bLookGoalYaw = false;
	bool bLookGoalPitch = false;
	float LookGoalYaw = 0.f;
	float LookGoalPitch = 0.f;
	bool bLookTrack = false;
	FGuid LookTrackSeatId;
	bool bLookStickyValid = false;
	FVector LookSticky = FVector::ZeroVector;
	float LookTrackReactRemaining = 0.f;
	float RecoilCorrectRemaining = 0.f;
	bool bRecoilPitchSlow = false;
	void TickAgentLook(float DeltaSeconds);
	void SlewControlToward(const FRotator& Desired, float DeltaSeconds, bool bSlewPitch, float PitchRateDegPerSec);

	void ConsumeAgentPulses();

	UFUNCTION()
	void HandleDamaged(float RemainingHealth, float Applied);

	UFUNCTION()
	void HandleDeath();

	void RequestTakeOutRespawn();
	void TickKnifeSlash(float DeltaSeconds);

	bool bTakenOut = false;
	TMap<TWeakObjectPtr<AController>, float> DamageTimes;
	TWeakObjectPtr<AController> LastDamageInstigator;
	FTimerHandle RespawnTimer;

	UPROPERTY()
	TObjectPtr<USceneComponent> HandR;

	UPROPERTY()
	TObjectPtr<USceneComponent> HandL;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ViewKnife;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> WorldKnife;

	float KnifeSlashRemaining = 0.f;

	float HurtAge = 100.f;
	FRotator BodyFlinchRot = FRotator::ZeroRotator;
	float ThirdPersonAlpha = 0.f;
	float ThirdPersonHold = 0.f;
	void UpdateThirdPersonPeek(float DeltaSeconds);

public:
	void AccumulateInput(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire);
};
