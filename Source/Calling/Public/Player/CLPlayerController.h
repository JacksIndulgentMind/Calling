#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/CLInputTypes.h"
#include "CLPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UCLMainMenuOverlay;
class UCLCombatHudWidget;
class UCLInputBindSubsystem;
struct FInputActionValue;

/**
 * Samples Enhanced Input every render frame into the pawn accumulators.
 * Remappable verbs come from UCLInputBindSubsystem (primary + secondary + gamepad chords).
 */
UCLASS()
class CALLING_API ACLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACLPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|UI")
	void ToggleMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Calling|UI")
	void SetMainMenuOpen(bool bOpen);

	UCLMainMenuOverlay* GetMainMenu();
	const UCLMainMenuOverlay* GetMainMenu() const { return MainMenuInstance; }

protected:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Calling|UI")
	TSubclassOf<UCLMainMenuOverlay> MainMenuClass;

	UPROPERTY()
	TObjectPtr<UCLMainMenuOverlay> MainMenuInstance;

	UPROPERTY()
	TObjectPtr<UCLCombatHudWidget> CombatHud;

	void EnsureCombatHud();
	void EnsureMainMenu();
	void ApplyMenuInputMode(bool bOpen);

	FVector2D CurrentMove = FVector2D::ZeroVector;
	FVector2D CurrentLook = FVector2D::ZeroVector;
	bool bSprint = false;
	bool bCrouch = false;
	bool bADS = false;
	bool bFire = false;
	bool bNativeForward = false;
	bool bNativeBack = false;
	bool bNativeLeft = false;
	bool bNativeRight = false;
	bool bMenuKeyLatched = false;

	void PushInputToPawn();
	void BindMoveLookKeys();
	void SampleGamepadAxes(float DeltaTime);
	void PollMenuKeys();
	void RestoreLitView();
	void RebuildNativeMove();
	void NativeForwardPressed();
	void NativeForwardReleased();
	void NativeBackPressed();
	void NativeBackReleased();
	void NativeLeftPressed();
	void NativeLeftReleased();
	void NativeRightPressed();
	void NativeRightReleased();
	void NativeLookYaw(float Value);
	void NativeLookPitch(float Value);

	void SampleRemappableBinds();
	void FirePulse(ECLBindableAction Action);
	bool ChordHeld(const FCLKeyChord& Chord, bool bAltDown) const;
	bool ChordPressed(const FCLKeyChord& Chord, bool bAltDown) const;
	UCLInputBindSubsystem* GetBinds() const;
};
