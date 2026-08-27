#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DLCombatHudWidget.generated.h"

class UOverlay;
class UOverlaySlot;
class UHorizontalBox;
class UVerticalBox;
class UBorder;
class UTextBlock;
class USizeBox;
class UProgressBar;
class ADLPlayerCharacter;

struct FDLRadarTrack
{
	TWeakObjectPtr<AActor> Actor;
	float Alpha = 0.f;
	float ScatterYawDeg = 0.f;
	float ScatterRadial = 0.f;
	FVector2D LastOffset = FVector2D::ZeroVector;
	bool bHasOffset = false;
	TArray<FVector2D> Trail;
};

struct FDLRadarPaintBlip
{
	FVector2D Offset = FVector2D::ZeroVector;
	TArray<FVector2D> Trail;
	FLinearColor Color = FLinearColor::White;
	float Alpha = 0.f;
	float Size = 3.f;
};

/** Quiet combat HUD: ability state bottom-left, gun strip bottom-center. No tutorials. */
UCLASS()
class DESTINYLIKE_API UDLCombatHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
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
	void RefreshSelfVitals(const ADLPlayerCharacter* Char);
	void RefreshSightedTarget(const ADLPlayerCharacter* Viewer, float DeltaTime);
	void RefreshRadar(const ADLPlayerCharacter* Viewer, float DeltaTime);
	const ADLPlayerCharacter* ResolveHudCharacter() const;
	FVector2D ResolveLayoutSize(const FGeometry* TickGeometry) const;
	void ApplyLayout(const FVector2D& ViewportSize);
	void PaintSightCrosshair(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintHurtVignette(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintRadar(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

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
	TWeakObjectPtr<AActor> SightedActor;
	float SightedHealth = 0.f;
	float SightedShield = 0.f;
	float SightedMaxHealth = 100.f;
	float SightedMaxShield = 0.f;
	float SightedSeenAge = 0.f;
	float SightedLostAge = 0.f;
	float SightedAlpha = 0.f;
	TArray<FDLRadarTrack> RadarTracks;
	TArray<FDLRadarPaintBlip> RadarBlips;
	float RadarScatterClock = 0.f;
	float RadarWedge[3] = {};
};
