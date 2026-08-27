#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Core/DLTypes.h"
#include "Input/DLInputTypes.h"
#include "DLMainMenuOverlay.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class UBorder;
class UTextBlock;
class UButton;
class UDLMainMenuOverlay;

UCLASS()
class DESTINYLIKE_API UDLBindSlotButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UDLMainMenuOverlay* InOwner, EDLBindableAction InAction, EDLBindColumn InColumn, bool bInClear);

	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UDLMainMenuOverlay> Owner;
	EDLBindableAction Action = EDLBindableAction::Fire;
	EDLBindColumn Column = EDLBindColumn::Primary;
	bool bClear = false;
};

/**
 * Director + keybinds overlay. I / Esc / F1 / Start (not remappable). Activities from here; remaps save immediately.
 */
UCLASS()
class DESTINYLIKE_API UDLMainMenuOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void ToggleOverlay();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void ShowOverlay();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void HideOverlay();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void JumpToActivity(EDLSceneId Scene, int32 RaidChamberIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void ExitToSocial();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void UnsetDefaultProfile();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void HostSocialLobby(EDLSocialPvpMode Mode);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void RefreshActivityLobbies(EDLSceneId Activity);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Menu")
	void JoinListedLobby(int32 Index);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Menu")
	bool IsOverlayVisible() const { return bVisible; }

	void ShowDirectorTab();
	void ShowKeybindsTab();

	bool IsListening() const { return bListening; }
	void CancelListen();
	void HandleBindSlotClicked(EDLBindableAction Action, EDLBindColumn Column, bool bClear);

	UFUNCTION()
	void HandleSessionEvent(bool bSuccess, const FString& Message);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void BuildWidgetTree();
	void BuildDirectorPanel(class UVerticalBox* RootCol);
	void BuildKeybindEditor(class UVerticalBox* RootCol);
	void RefreshBindRows();
	void SetListenPrompt(const FString& Text, bool bWarning);
	void ProposeChord(const FDLKeyChord& Chord);
	bool TryAcceptProposed();

	UFUNCTION()
	void HandleDirectorTabClicked();

	UFUNCTION()
	void HandleKeybindsTabClicked();

	UFUNCTION()
	void HandlePvpClicked();

	UFUNCTION()
	void HandleComposeClicked();

	UFUNCTION()
	void HandleJoinRedClicked();

	UFUNCTION()
	void HandleJoinBlueClicked();

	UFUNCTION()
	void HandleRaidClicked();

	UFUNCTION()
	void HandlePracticeClicked();

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleGoClicked();

	UFUNCTION()
	void HandleHostSocialOpenClicked();

	UFUNCTION()
	void HandleHostSocialClosedClicked();

	UFUNCTION()
	void HandleExitSocialClicked();

	UFUNCTION()
	void HandleAcceptBindClicked();

	UFUNCTION()
	void HandleCancelListenClicked();

	UFUNCTION()
	void HandleRebindYesClicked();

	UFUNCTION()
	void HandleRebindNoClicked();

	UFUNCTION()
	void HandleResetDefaultsClicked();

	UFUNCTION()
	void HandleDoneKeybindsClicked();

	UPROPERTY()
	bool bVisible = false;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UVerticalBox> DirectorBox;

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
	EDLBindableAction ListenAction = EDLBindableAction::Fire;
	EDLBindColumn ListenColumn = EDLBindColumn::Primary;
	FDLKeyChord ProposedChord;
	FDLBindUse DisplacedUse;
};
