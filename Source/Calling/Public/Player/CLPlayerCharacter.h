#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/CLTypes.h"
#include "Input/CLAgentIntent.h"
#include "TimerManager.h"
#include "CLPlayerCharacter.generated.h"

class UCLCombatMovementComponent;
class UCLHealthShieldComponent;
class UCLWeaponMotorComponent;
class UCLWeaponBehaviorComponent;
class UCLAbilityLoadoutComponent;
class UCLPossessionComponent;
class UCLLookController;
class UCLIntentReceiver;
class UCLDamageableComponent;
class UCLEffectStackComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class CALLING_API ACLPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACLPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Jump() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	UFUNCTION(BlueprintCallable, Category = "Calling")
	void ApplyProfileLoadout();

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLCombatMovementComponent* GetCombatMovement() const { return CombatMovement; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLHealthShieldComponent* GetHealthShield() const { return HealthShield; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLDamageableComponent* GetDamageable() const { return Damageable; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLEffectStackComponent* GetEffectStack() const { return EffectStack; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLWeaponMotorComponent* GetWeaponMotor() const { return WeaponMotor; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLAbilityLoadoutComponent* GetAbilities() const { return Abilities; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLPossessionComponent* GetPossession() const { return Possession; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLLookController* GetLookController() const { return LookController; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	UCLIntentReceiver* GetIntentReceiver() const { return IntentReceiver; }

	UFUNCTION(BlueprintPure, Category = "Calling")
	bool IsSliding() const;

	UFUNCTION(BlueprintPure, Category = "Calling")
	UStaticMeshComponent* GetBodyMesh() const { return BodyMesh; }

	USceneComponent* GetViewWeaponRoot() const { return ViewWeaponRoot; }
	USceneComponent* GetWorldWeaponRoot() const { return WorldWeaponRoot; }
	bool UsesViewWeapon() const;
	float GetThirdPersonAlpha() const { return ThirdPersonAlpha; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	virtual void SetDemoViewActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Calling")
	float GetHurtAlpha() const;

	FVector2D GetAdsReticlePunch() const;

	void SetRippleCamo(bool bEnabled);

	/** Shared intent seam: HTTP, /goto, and in-game playbooks. Holdables latch; look/pulses consume. */
	void ApplyAgentIntent(const FCLAgentIntent& Intent);

	void ApplyAgentIntent(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire,
		bool bJump, bool bDodge, bool bDash, bool bReload, bool bSwap);

	/** Zero stick and pending pulses. Sequence / goto / empty intent call this. */
	void ClearAgentIntent();

	void ApplyAgentLookCommand(const FCLLookCommand& Look);
	void ApplyAgentLookFromStep(const FGuid& TrackSeatId, const FCLLookCommand& Look);
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLCombatMovementComponent> CombatMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLHealthShieldComponent> HealthShield;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLDamageableComponent> Damageable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLEffectStackComponent> EffectStack;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLWeaponBehaviorComponent> WeaponBehavior;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLWeaponMotorComponent> WeaponMotor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLAbilityLoadoutComponent> Abilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLPossessionComponent> Possession;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLLookController> LookController;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCLIntentReceiver> IntentReceiver;

	UPROPERTY()
	bool bUseNpcLoadout = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Calling")
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
