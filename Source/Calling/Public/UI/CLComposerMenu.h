#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "CLComposerMenu.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UButton;
class UBorder;
class UComboBoxString;

/** Composer-scene menu: seats, local Ready (host or guest), host Start. Hub ready/go is the net twin. */
UCLASS()
class CALLING_API UCLComposerMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleReadyClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleGuestClicked();

	UFUNCTION()
	void HandleJoinRedClicked();

	UFUNCTION()
	void HandleJoinBlueClicked();

	UFUNCTION()
	void HandleVirtualHostClicked();

	UFUNCTION()
	void HandleVirtualJoinClicked();

protected:
	void BuildWidgetTree();
	void Refresh();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> RoleLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> SeatListLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> ReadyButtonLabel;

	UPROPERTY()
	TObjectPtr<UButton> ReadyButton;

	UPROPERTY()
	TObjectPtr<UButton> StartButton;

	UPROPERTY()
	TObjectPtr<UButton> HostButton;

	UPROPERTY()
	TObjectPtr<UButton> GuestButton;

	UPROPERTY()
	TObjectPtr<UButton> RedButton;

	UPROPERTY()
	TObjectPtr<UButton> BlueButton;

	UPROPERTY()
	TObjectPtr<UButton> VirtualHostButton;

	UPROPERTY()
	TObjectPtr<UButton> VirtualJoinButton;

	UPROPERTY()
	TObjectPtr<UComboBoxString> JoinSourceCombo;

	float RefreshAccum = 0.f;
	FTimerHandle RefreshTimer;
};
