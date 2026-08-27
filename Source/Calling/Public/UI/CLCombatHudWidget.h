#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CLCombatHudWidget.generated.h"

class UOverlay;
class UOverlaySlot;
class UHorizontalBox;
class UVerticalBox;
class UBorder;
class UTextBlock;
class USizeBox;
class UProgressBar;
class ACLPlayerCharacter;
class UCLHudVitalsPanel;
class UCLHudRadarController;
class UCLHudPainter;

/** Quiet combat HUD: ability state bottom-left, gun strip bottom-center. No tutorials. */
UCLASS()
class CALLING_API UCLCombatHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCLCombatHudWidget(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	void Refresh(float DeltaTime = 0.f);
	void PinToGameViewport();

protected:
	void BuildWidgetTree();
	void RefreshFromPawn(float DeltaTime);
	const ACLPlayerCharacter* ResolveHudCharacter() const;
	FVector2D ResolveLayoutSize(const FGeometry* TickGeometry) const;
	void ApplyLayout(const FVector2D& ViewportSize);

	UPROPERTY()
	TObjectPtr<UCLHudVitalsPanel> Vitals;

	UPROPERTY()
	TObjectPtr<UCLHudRadarController> Radar;

	UPROPERTY()
	TObjectPtr<UCLHudPainter> Painter;

	UPROPERTY()
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY()
	TObjectPtr<UVerticalBox> LeftStack;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> SlotRow;

	UPROPERTY()
	TObjectPtr<UOverlaySlot> SlotRowSlot;

	UPROPERTY()
	TObjectPtr<USizeBox> SelfVitalsBox;

	UPROPERTY()
	TObjectPtr<USizeBox> SelfShieldBox;

	UPROPERTY()
	TObjectPtr<USizeBox> SelfHealthBox;

	UPROPERTY()
	TObjectPtr<UProgressBar> SelfShieldBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> SelfHealthBar;

	UPROPERTY()
	TObjectPtr<USizeBox> SightedBox;

	UPROPERTY()
	TObjectPtr<UOverlaySlot> SightedSlot;

	UPROPERTY()
	TObjectPtr<USizeBox> SightedShieldBox;

	UPROPERTY()
	TObjectPtr<USizeBox> SightedHealthBox;

	UPROPERTY()
	TObjectPtr<UProgressBar> SightedShieldBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> SightedHealthBar;

	UPROPERTY()
	TObjectPtr<UOverlaySlot> GunSlot;

	UPROPERTY()
	TObjectPtr<UTextBlock> GunLine;

	UPROPERTY()
	TObjectPtr<UTextBlock> ScoreLine;

	UPROPERTY()
	TObjectPtr<UOverlaySlot> ScoreSlot;

	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> SlotBoxes;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotBorders;

	UPROPERTY()
	TArray<TObjectPtr<UProgressBar>> SlotFills;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotGlyphs;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotKeys;

	bool bShowKeybinds = false;
	FVector2D LastTickSize = FVector2D::ZeroVector;
};
