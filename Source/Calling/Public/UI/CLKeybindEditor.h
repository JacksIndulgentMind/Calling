#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/Button.h"
#include "Input/CLInputTypes.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "CLKeybindEditor.generated.h"

class UCLKeybindEditor;
class UCLMainMenuOverlay;
class UScrollBox;
class UBorder;
class UTextBlock;
class UHorizontalBox;
class UVerticalBox;

UCLASS()
class CALLING_API UCLBindSlotButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UCLKeybindEditor* InOwner, ECLBindableAction InAction, ECLBindColumn InColumn, bool bInClear);

	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UCLKeybindEditor> Owner;
	ECLBindableAction Action = ECLBindableAction::Fire;
	ECLBindColumn Column = ECLBindColumn::Primary;
	bool bClear = false;
};

/** Listen/accept/rebind editor for action chords. */
UCLASS()
class CALLING_API UCLKeybindEditor : public UObject
{
	GENERATED_BODY()

public:
	void BindWidgets(UVerticalBox* InKeybindsBox, UScrollBox* InBindScroll, UBorder* InListenPanel, UTextBlock* InListenText,
		UHorizontalBox* InListenButtons, UBorder* InRebindPanel, UTextBlock* InRebindText,
		TArray<TObjectPtr<UTextBlock>> InBindLabels, TArray<TObjectPtr<UTextBlock>> InPrimaryLabels,
		TArray<TObjectPtr<UTextBlock>> InSecondaryLabels, TArray<TObjectPtr<UTextBlock>> InGamepadLabels);

	bool IsListening() const { return bListening; }
	void CancelListen();
	void HandleBindSlotClicked(ECLBindableAction Action, ECLBindColumn Column, bool bClear);
	void RefreshBindRows();
	void TickListen();
	FReply OnKeyDown(const FKeyEvent& InKeyEvent);
	FReply OnMouseButtonDown(const FPointerEvent& InMouseEvent);
	FReply OnMouseWheel(const FPointerEvent& InMouseEvent);

	void HandleAcceptBindClicked();
	void HandleCancelListenClicked();
	void HandleRebindYesClicked();
	void HandleRebindNoClicked();
	void HandleResetDefaultsClicked();
	void HideRebindPanel();

protected:
	void SetListenPrompt(const FString& Text, bool bWarning);
	void ProposeChord(const FCLKeyChord& Chord);
	bool TryAcceptProposed();
	UCLMainMenuOverlay* GetOverlay() const;

	UPROPERTY()
	TObjectPtr<UVerticalBox> KeybindsBox;

	UPROPERTY()
	TObjectPtr<UScrollBox> BindScroll;

	UPROPERTY()
	TObjectPtr<UBorder> ListenPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> ListenText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> ListenButtons;

	UPROPERTY()
	TObjectPtr<UBorder> RebindPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> RebindText;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> BindLabels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PrimaryLabels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SecondaryLabels;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> GamepadLabels;

	bool bListening = false;
	bool bHasProposed = false;
	ECLBindableAction ListenAction = ECLBindableAction::Fire;
	ECLBindColumn ListenColumn = ECLBindColumn::Primary;
	FCLKeyChord ProposedChord;
	FCLBindUse DisplacedUse;
};
