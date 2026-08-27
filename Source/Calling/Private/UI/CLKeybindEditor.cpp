#include "UI/CLKeybindEditor.h"
#include "UI/CLMainMenuOverlay.h"
#include "Game/CLInputBindSubsystem.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Engine/GameInstance.h"

namespace
{
	UCLInputBindSubsystem* BindTable(const UUserWidget* Widget)
	{
		const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCLInputBindSubsystem>() : nullptr;
	}
}

void UCLBindSlotButton::Configure(UCLKeybindEditor* InOwner, ECLBindableAction InAction, ECLBindColumn InColumn, bool bInClear)
{
	Owner = InOwner;
	Action = InAction;
	Column = InColumn;
	bClear = bInClear;
	OnClicked.AddDynamic(this, &UCLBindSlotButton::HandleClicked);
}

void UCLBindSlotButton::HandleClicked()
{
	if (UCLKeybindEditor* Editor = Owner.Get())
	{
		Editor->HandleBindSlotClicked(Action, Column, bClear);
	}
}

void UCLKeybindEditor::BindWidgets(UVerticalBox* InKeybindsBox, UScrollBox* InBindScroll, UBorder* InListenPanel, UTextBlock* InListenText,
	UHorizontalBox* InListenButtons, UBorder* InRebindPanel, UTextBlock* InRebindText,
	TArray<TObjectPtr<UTextBlock>> InBindLabels, TArray<TObjectPtr<UTextBlock>> InPrimaryLabels,
	TArray<TObjectPtr<UTextBlock>> InSecondaryLabels, TArray<TObjectPtr<UTextBlock>> InGamepadLabels)
{
	KeybindsBox = InKeybindsBox;
	BindScroll = InBindScroll;
	ListenPanel = InListenPanel;
	ListenText = InListenText;
	ListenButtons = InListenButtons;
	RebindPanel = InRebindPanel;
	RebindText = InRebindText;
	BindLabels = MoveTemp(InBindLabels);
	PrimaryLabels = MoveTemp(InPrimaryLabels);
	SecondaryLabels = MoveTemp(InSecondaryLabels);
	GamepadLabels = MoveTemp(InGamepadLabels);
}

UCLMainMenuOverlay* UCLKeybindEditor::GetOverlay() const
{
	return Cast<UCLMainMenuOverlay>(GetOuter());
}

void UCLKeybindEditor::RefreshBindRows()
{
	UCLInputBindSubsystem* Table = BindTable(GetOverlay());
	if (!Table)
	{
		return;
	}
	const TArray<ECLBindableAction>& Actions = CLInput::AllActions();
	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		const FCLActionBinds Pair = Table->GetBinds(Actions[i]);
		if (PrimaryLabels.IsValidIndex(i) && PrimaryLabels[i])
		{
			PrimaryLabels[i]->SetText(FText::FromString(Pair.Primary.ToDisplayString()));
		}
		if (SecondaryLabels.IsValidIndex(i) && SecondaryLabels[i])
		{
			SecondaryLabels[i]->SetText(FText::FromString(Pair.Secondary.ToDisplayString()));
		}
		if (GamepadLabels.IsValidIndex(i) && GamepadLabels[i])
		{
			GamepadLabels[i]->SetText(FText::FromString(Pair.Gamepad.ToDisplayString()));
		}
		if (BindLabels.IsValidIndex(i) && BindLabels[i])
		{
			const bool bUnbound = Pair.IsUnbound();
			BindLabels[i]->SetColorAndOpacity(FSlateColor(bUnbound
				? FLinearColor(1.f, 0.72f, 0.2f)
				: FLinearColor::White));
		}
	}
}

void UCLKeybindEditor::SetListenPrompt(const FString& Text, bool bWarning)
{
	if (ListenText)
	{
		ListenText->SetText(FText::FromString(Text));
		ListenText->SetColorAndOpacity(FSlateColor(bWarning
			? FLinearColor(1.f, 0.75f, 0.25f)
			: FLinearColor::White));
	}
}

void UCLKeybindEditor::HandleBindSlotClicked(ECLBindableAction Action, ECLBindColumn Column, bool bClear)
{
	UCLInputBindSubsystem* Table = BindTable(GetOverlay());
	if (!Table)
	{
		return;
	}
	if (bClear)
	{
		Table->ClearBind(Action, Column);
		RefreshBindRows();
		return;
	}

	bListening = true;
	bHasProposed = false;
	ListenAction = Action;
	ListenColumn = Column;
	ProposedChord = FCLKeyChord();
	if (ListenPanel)
	{
		ListenPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (ListenButtons)
	{
		ListenButtons->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetListenPrompt(FString::Printf(TEXT("Press a key for %s (%s). Esc cancels. Alt + key for a combo. Triggers click past threshold."),
		*CLInput::ActionDisplayName(Action),
		*CLInput::ColumnDisplayName(Column)), false);
	if (UCLMainMenuOverlay* Overlay = GetOverlay())
	{
		Overlay->SetKeyboardFocus();
	}
}

void UCLKeybindEditor::CancelListen()
{
	bListening = false;
	bHasProposed = false;
	ProposedChord = FCLKeyChord();
	if (ListenPanel)
	{
		ListenPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ListenButtons)
	{
		ListenButtons->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCLKeybindEditor::ProposeChord(const FCLKeyChord& Chord)
{
	if (!Chord.IsSet())
	{
		return;
	}
	ProposedChord = Chord;
	bHasProposed = true;

	UCLInputBindSubsystem* Table = BindTable(GetOverlay());
	FString Warn;
	bool bWarning = false;
	if (Table)
	{
		const TArray<FCLBindUse> Uses = Table->FindUses(Chord);
		for (const FCLBindUse& Use : Uses)
		{
			if (Use.Action == ListenAction && Use.Column == ListenColumn)
			{
				continue;
			}
			bWarning = true;
			const FCLBindUse SameCol = Table->FindSameColumnUse(Chord, ListenColumn, ListenAction);
			if (SameCol.bValid)
			{
				Warn = FString::Printf(TEXT("%s is used for %s (%s). Accept will unbind that column."),
					*Chord.ToDisplayString(),
					*CLInput::ActionDisplayName(SameCol.Action),
					*CLInput::ColumnDisplayName(SameCol.Column));
			}
			else
			{
				Warn = FString::Printf(TEXT("%s is also bound to %s (%s). Both will fire the same tick if they can."),
					*Chord.ToDisplayString(),
					*CLInput::ActionDisplayName(Use.Action),
					*CLInput::ColumnDisplayName(Use.Column));
			}
			break;
		}
	}

	if (Warn.IsEmpty())
	{
		SetListenPrompt(FString::Printf(TEXT("Bind %s to %s (%s)?"),
			*Chord.ToDisplayString(),
			*CLInput::ActionDisplayName(ListenAction),
			*CLInput::ColumnDisplayName(ListenColumn)), false);
	}
	else
	{
		SetListenPrompt(Warn, bWarning);
	}
	if (ListenButtons)
	{
		ListenButtons->SetVisibility(ESlateVisibility::Visible);
	}
}

bool UCLKeybindEditor::TryAcceptProposed()
{
	UCLInputBindSubsystem* Table = BindTable(GetOverlay());
	if (!Table || !bHasProposed || !ProposedChord.IsSet())
	{
		return false;
	}

	FCLBindUse Stolen;
	const bool bStole = Table->SetBind(ListenAction, ListenColumn, ProposedChord, Stolen);
	CancelListen();
	RefreshBindRows();
	if (bStole && Stolen.bValid)
	{
		DisplacedUse = Stolen;
		if (RebindText)
		{
			RebindText->SetText(FText::FromString(FString::Printf(
				TEXT("Would you like to now re-bind the other key? (%s %s)"),
				*CLInput::ActionDisplayName(Stolen.Action),
				*CLInput::ColumnDisplayName(Stolen.Column))));
		}
		if (RebindPanel)
		{
			RebindPanel->SetVisibility(ESlateVisibility::Visible);
		}
		return true;
	}
	return true;
}

void UCLKeybindEditor::TickListen()
{
	if (!bListening)
	{
		return;
	}
	UCLMainMenuOverlay* Overlay = GetOverlay();
	APlayerController* PC = Overlay ? Overlay->GetOwningPlayer() : nullptr;
	if (!PC)
	{
		return;
	}
	static const FKey PadKeys[] = {
		EKeys::Gamepad_FaceButton_Bottom,
		EKeys::Gamepad_FaceButton_Right,
		EKeys::Gamepad_FaceButton_Left,
		EKeys::Gamepad_FaceButton_Top,
		EKeys::Gamepad_LeftShoulder,
		EKeys::Gamepad_RightShoulder,
		EKeys::Gamepad_LeftTrigger,
		EKeys::Gamepad_RightTrigger,
		EKeys::Gamepad_LeftThumbstick,
		EKeys::Gamepad_RightThumbstick,
		EKeys::Gamepad_DPad_Up,
		EKeys::Gamepad_DPad_Down,
		EKeys::Gamepad_DPad_Left,
		EKeys::Gamepad_DPad_Right
	};
	for (const FKey& Key : PadKeys)
	{
		if (PC->WasInputKeyJustPressed(Key))
		{
			FCLKeyChord Chord;
			Chord.Key = Key;
			ProposeChord(Chord);
			return;
		}
	}
}

FReply UCLKeybindEditor::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	if (!bListening)
	{
		return FReply::Unhandled();
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		CancelListen();
		return FReply::Handled();
	}
	if (CLInput::IsAltKey(Key) || CLInput::IsReservedMenuKey(Key))
	{
		return FReply::Handled();
	}

	FCLKeyChord Chord;
	Chord.Key = Key;
	Chord.bAlt = InKeyEvent.IsAltDown();
	ProposeChord(Chord);
	return FReply::Handled();
}

FReply UCLKeybindEditor::OnMouseButtonDown(const FPointerEvent& InMouseEvent)
{
	if (!bListening)
	{
		return FReply::Unhandled();
	}

	FCLKeyChord Chord;
	Chord.Key = InMouseEvent.GetEffectingButton();
	Chord.bAlt = InMouseEvent.IsAltDown();
	if (CLInput::IsAltKey(Chord.Key))
	{
		return FReply::Handled();
	}
	ProposeChord(Chord);
	return FReply::Handled();
}

FReply UCLKeybindEditor::OnMouseWheel(const FPointerEvent& InMouseEvent)
{
	if (!bListening)
	{
		return FReply::Unhandled();
	}

	FCLKeyChord Chord;
	Chord.Key = InMouseEvent.GetWheelDelta() >= 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown;
	Chord.bAlt = InMouseEvent.IsAltDown();
	ProposeChord(Chord);
	return FReply::Handled();
}

void UCLKeybindEditor::HandleAcceptBindClicked()
{
	TryAcceptProposed();
}

void UCLKeybindEditor::HandleCancelListenClicked()
{
	CancelListen();
}

void UCLKeybindEditor::HandleRebindYesClicked()
{
	if (RebindPanel)
	{
		RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DisplacedUse.bValid)
	{
		HandleBindSlotClicked(DisplacedUse.Action, DisplacedUse.Column, false);
	}
	DisplacedUse = FCLBindUse();
}

void UCLKeybindEditor::HandleRebindNoClicked()
{
	HideRebindPanel();
	DisplacedUse = FCLBindUse();
}

void UCLKeybindEditor::HandleResetDefaultsClicked()
{
	if (UCLInputBindSubsystem* Table = BindTable(GetOverlay()))
	{
		Table->ResetDefaults();
		RefreshBindRows();
		CancelListen();
		HideRebindPanel();
	}
}

void UCLKeybindEditor::HideRebindPanel()
{
	if (RebindPanel)
	{
		RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
