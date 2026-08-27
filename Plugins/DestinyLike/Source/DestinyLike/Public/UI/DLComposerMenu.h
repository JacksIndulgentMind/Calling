#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "DLComposerMenu.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UButton;
class UBorder;

/** Composer-scene menu: seats, local Ready (host or guest), host Start. Hub ready/go is the net twin. */
UCLASS()
class DESTINYLIKE_API UDLComposerMenu : public UUserWidget
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

	float RefreshAccum = 0.f;
	FTimerHandle RefreshTimer;
};
