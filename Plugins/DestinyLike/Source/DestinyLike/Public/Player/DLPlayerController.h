#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/DLInputTypes.h"
#include "DLPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UDLMainMenuOverlay;
class UDLCombatHudWidget;
class UDLInputBindSubsystem;
struct FInputActionValue;

/**
 * Samples Enhanced Input every render frame into the pawn accumulators.
 * Remappable verbs come from UDLInputBindSubsystem (primary + secondary + gamepad chords).
 */
UCLASS()
class DESTINYLIKE_API ADLPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADLPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|UI")
	void ToggleMainMenu();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|UI")
	void SetMainMenuOpen(bool bOpen);

	UDLMainMenuOverlay* GetMainMenu();
	const UDLMainMenuOverlay* GetMainMenu() const { return MainMenuInstance; }

protected:
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DestinyLike|UI")
	TSubclassOf<UDLMainMenuOverlay> MainMenuClass;

	UPROPERTY()
	TObjectPtr<UDLMainMenuOverlay> MainMenuInstance;

	UPROPERTY()
	TObjectPtr<UDLCombatHudWidget> CombatHud;

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
	void FirePulse(EDLBindableAction Action);
	bool ChordHeld(const FDLKeyChord& Chord, bool bAltDown) const;
	bool ChordPressed(const FDLKeyChord& Chord, bool bAltDown) const;
	UDLInputBindSubsystem* GetBinds() const;
};
