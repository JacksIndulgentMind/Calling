#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/CLTypes.h"
#include "Game/CLLobbyTypes.h"
#include "Input/CLInputTypes.h"
#include "CLMainMenuOverlay.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UCLDirectorPanel;
class UCLKeybindEditor;

/**
 * Director + keybinds overlay. I / Esc / F1 / Start (not remappable). Activities from here; remaps save immediately.
 */
UCLASS()
class CALLING_API UCLMainMenuOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	UCLMainMenuOverlay(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void ToggleOverlay();

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void ShowOverlay();

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void HideOverlay();

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void JumpToActivity(ECLSceneId Scene, int32 RaidChamberIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void ExitToSocial();

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void UnsetDefaultProfile();

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void HostSocialLobby(ECLSocialPvpMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void RefreshActivityLobbies(ECLSceneId Activity);

	UFUNCTION(BlueprintCallable, Category = "Calling|Menu")
	void JoinListedLobby(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Calling|Menu")
	bool IsOverlayVisible() const { return bVisible; }

	void ShowDirectorTab();
	void ShowKeybindsTab();

	bool IsListening() const;
	void CancelListen();
	void HandleBindSlotClicked(ECLBindableAction Action, ECLBindColumn Column, bool bClear);

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
	TObjectPtr<UCLDirectorPanel> DirectorPanel;

	UPROPERTY()
	TObjectPtr<UCLKeybindEditor> KeybindEditor;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UVerticalBox> DirectorBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> KeybindsBox;
};
