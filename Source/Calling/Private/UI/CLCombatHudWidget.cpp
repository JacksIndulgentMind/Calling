#include "UI/CLCombatHudWidget.h"
#include "UI/CLHudVitalsPanel.h"
#include "UI/CLHudRadarController.h"
#include "UI/CLHudPainter.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLWeaponMotorComponent.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLLobbySubsystem.h"
#include "Ability/CLAbility.h"
#include "Ability/CLAbilityTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Game/CLInputBindSubsystem.h"
#include "Input/CLInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/Layout/Anchors.h"

namespace
{
	struct FHudSlotDef
	{
		const TCHAR* Glyph;
		ECLAbilitySlot AbilitySlot;
		bool bDodge;
		ECLBindableAction BindAction;
	};

	const FHudSlotDef HudSlots[] = {
		{ TEXT("●"), ECLAbilitySlot::Grenade, false, ECLBindableAction::Grenade },
		{ TEXT("†"), ECLAbilitySlot::Melee, false, ECLBindableAction::Melee },
		{ TEXT("»"), ECLAbilitySlot::Dash, false, ECLBindableAction::Dash },
		{ TEXT("▣"), ECLAbilitySlot::Shield, false, ECLBindableAction::Shield },
		{ TEXT("◌"), ECLAbilitySlot::Evasion, false, ECLBindableAction::Evasion },
		{ TEXT("↷"), ECLAbilitySlot::Grenade, true, ECLBindableAction::Dodge },
	};

	constexpr int32 HudSlotCount = UE_ARRAY_COUNT(HudSlots);

	const FLinearColor ShieldFill(0.55f, 0.82f, 0.98f, 0.95f);
	const FLinearColor HealthFill(0.86f, 0.22f, 0.18f, 0.95f);
	const FLinearColor VitalBg(0.06f, 0.06f, 0.07f, 0.78f);

	UProgressBar* MakeVitalBar(UWidgetTree* Tree, const FName Name, const FLinearColor& Fill)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		Bar->SetPercent(1.f);
		Bar->SetBarFillType(EProgressBarFillType::LeftToRight);
		Bar->SetFillColorAndOpacity(Fill);
		return Bar;
	}

	USizeBox* MakeVitalLane(UWidgetTree* Tree, UVerticalBox* Parent, UProgressBar* Bar, const FName BoxName, float BottomPad)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), BoxName);
		UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sFrame"), *BoxName.ToString()));
		Frame->SetBrushColor(VitalBg);
		Frame->SetPadding(FMargin(0.f));
		Frame->AddChild(Bar);
		Box->AddChild(Frame);
		if (UVerticalBoxSlot* Lane = Parent->AddChildToVerticalBox(Box))
		{
			Lane->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPad));
			Lane->SetHorizontalAlignment(HAlign_Fill);
		}
		return Box;
	}
}

UCLCombatHudWidget::UCLCombatHudWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Vitals = CreateDefaultSubobject<UCLHudVitalsPanel>(TEXT("Vitals"));
	Radar = CreateDefaultSubobject<UCLHudRadarController>(TEXT("Radar"));
	Painter = CreateDefaultSubobject<UCLHudPainter>(TEXT("Painter"));
}

TSharedRef<SWidget> UCLCombatHudWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UCLCombatHudWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HudRoot"));
	WidgetTree->RootWidget = RootOverlay;

	SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SlotRow"));
	SlotBoxes.SetNum(HudSlotCount);
	SlotBorders.SetNum(HudSlotCount);
	SlotFills.SetNum(HudSlotCount);
	SlotGlyphs.SetNum(HudSlotCount);
	SlotKeys.SetNum(HudSlotCount);

	for (int32 i = 0; i < HudSlotCount; ++i)
	{
		UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("SlotCol%d"), i));
		SlotBoxes[i] = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("SlotBox%d"), i));
		UOverlay* BoxOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("SlotOverlay%d"), i));

		SlotBorders[i] = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("SlotBorder%d"), i));
		SlotBorders[i]->SetBrushColor(FLinearColor(0.06f, 0.06f, 0.07f, 0.78f));
		SlotBorders[i]->SetPadding(FMargin(0.f));
		if (UOverlaySlot* BgSlot = BoxOverlay->AddChildToOverlay(SlotBorders[i]))
		{
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		SlotFills[i] = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), *FString::Printf(TEXT("SlotFill%d"), i));
		SlotFills[i]->SetPercent(1.f);
		SlotFills[i]->SetBarFillType(EProgressBarFillType::BottomToTop);
		SlotFills[i]->SetFillColorAndOpacity(FLinearColor(0.22f, 0.24f, 0.28f, 0.95f));
		if (UOverlaySlot* FillSlot = BoxOverlay->AddChildToOverlay(SlotFills[i]))
		{
			FillSlot->SetHorizontalAlignment(HAlign_Fill);
			FillSlot->SetVerticalAlignment(VAlign_Fill);
		}

		SlotGlyphs[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotGlyph%d"), i));
		SlotGlyphs[i]->SetText(FText::FromString(HudSlots[i].Glyph));
		SlotGlyphs[i]->SetJustification(ETextJustify::Center);
		FSlateFontInfo GlyphFont = SlotGlyphs[i]->GetFont();
		GlyphFont.Size = 18;
		SlotGlyphs[i]->SetFont(GlyphFont);
		SlotGlyphs[i]->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UOverlaySlot* GlyphSlot = BoxOverlay->AddChildToOverlay(SlotGlyphs[i]))
		{
			GlyphSlot->SetHorizontalAlignment(HAlign_Center);
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
		}

		SlotBoxes[i]->AddChild(BoxOverlay);
		Col->AddChildToVerticalBox(SlotBoxes[i]);

		SlotKeys[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SlotKey%d"), i));
		SlotKeys[i]->SetText(FText::FromString(TEXT("—")));
		SlotKeys[i]->SetJustification(ETextJustify::Center);
		FSlateFontInfo KeyFont = SlotKeys[i]->GetFont();
		KeyFont.Size = 10;
		SlotKeys[i]->SetFont(KeyFont);
		SlotKeys[i]->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
		SlotKeys[i]->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* KeySlot = Col->AddChildToVerticalBox(SlotKeys[i]))
		{
			KeySlot->SetHorizontalAlignment(HAlign_Center);
		}

		if (UHorizontalBoxSlot* RowSlot = SlotRow->AddChildToHorizontalBox(Col))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}

	LeftStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftStack"));
	UVerticalBox* SelfVitals = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SelfVitals"));
	SelfShieldBar = MakeVitalBar(WidgetTree, TEXT("SelfShieldBar"), ShieldFill);
	SelfHealthBar = MakeVitalBar(WidgetTree, TEXT("SelfHealthBar"), HealthFill);
	SelfShieldBox = MakeVitalLane(WidgetTree, SelfVitals, SelfShieldBar, TEXT("SelfShieldBox"), 3.f);
	SelfHealthBox = MakeVitalLane(WidgetTree, SelfVitals, SelfHealthBar, TEXT("SelfHealthBox"), 0.f);
	SelfVitalsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SelfVitalsBox"));
	SelfVitalsBox->AddChild(SelfVitals);
	if (UVerticalBoxSlot* VitalSlot = LeftStack->AddChildToVerticalBox(SelfVitalsBox))
	{
		VitalSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		VitalSlot->SetHorizontalAlignment(HAlign_Left);
	}
	LeftStack->AddChildToVerticalBox(SlotRow);

	if (UOverlaySlot* RowOverlaySlot = RootOverlay->AddChildToOverlay(LeftStack))
	{
		RowOverlaySlot->SetHorizontalAlignment(HAlign_Left);
		RowOverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		SlotRowSlot = RowOverlaySlot;
	}

	UVerticalBox* SightedVitals = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SightedVitals"));
	SightedShieldBar = MakeVitalBar(WidgetTree, TEXT("SightedShieldBar"), ShieldFill);
	SightedHealthBar = MakeVitalBar(WidgetTree, TEXT("SightedHealthBar"), HealthFill);
	SightedShieldBox = MakeVitalLane(WidgetTree, SightedVitals, SightedShieldBar, TEXT("SightedShieldBox"), 2.f);
	SightedHealthBox = MakeVitalLane(WidgetTree, SightedVitals, SightedHealthBar, TEXT("SightedHealthBox"), 0.f);
	SightedBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SightedBox"));
	SightedBox->AddChild(SightedVitals);
	SightedBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* SightOverlay = RootOverlay->AddChildToOverlay(SightedBox))
	{
		SightOverlay->SetHorizontalAlignment(HAlign_Right);
		SightOverlay->SetVerticalAlignment(VAlign_Bottom);
		SightedSlot = SightOverlay;
	}

	GunLine = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GunLine"));
	GunLine->SetText(FText::GetEmpty());
	FSlateFontInfo GunFont = GunLine->GetFont();
	GunFont.Size = 16;
	GunLine->SetFont(GunFont);
	GunLine->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UOverlaySlot* LineSlot = RootOverlay->AddChildToOverlay(GunLine))
	{
		LineSlot->SetHorizontalAlignment(HAlign_Center);
		LineSlot->SetVerticalAlignment(VAlign_Bottom);
		GunSlot = LineSlot;
	}

	ScoreLine = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScoreLine"));
	ScoreLine->SetText(FText::GetEmpty());
	FSlateFontInfo ScoreFont = ScoreLine->GetFont();
	ScoreFont.Size = 14;
	ScoreLine->SetFont(ScoreFont);
	ScoreLine->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.88f, 0.92f)));
	if (UOverlaySlot* ScoreOverlay = RootOverlay->AddChildToOverlay(ScoreLine))
	{
		ScoreOverlay->SetHorizontalAlignment(HAlign_Center);
		ScoreOverlay->SetVerticalAlignment(VAlign_Top);
		ScoreSlot = ScoreOverlay;
	}

	if (Vitals)
	{
		Vitals->Bind(SelfVitalsBox, SelfShieldBox, SelfHealthBox, SelfShieldBar, SelfHealthBar,
			SightedBox, SightedSlot, SightedShieldBox, SightedHealthBox, SightedShieldBar, SightedHealthBar);
	}
}

void UCLCombatHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	PinToGameViewport();
	Refresh();
}

int32 UCLCombatHudWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const ACLPlayerCharacter* Char = ResolveHudCharacter();
	if (Painter)
	{
		Painter->PaintHurtVignette(AllottedGeometry, OutDrawElements, Layer + 1, Char);
		Painter->PaintSightCrosshair(AllottedGeometry, OutDrawElements, Layer + 2, Char);
		if (Radar)
		{
			Painter->PaintRadar(AllottedGeometry, OutDrawElements, Layer + 3, Radar->GetBlips(), Radar->GetWedge(), Char != nullptr);
		}
	}
	return Layer + 4;
}

void UCLCombatHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastTickSize = MyGeometry.GetLocalSize();
	Refresh(InDeltaTime);
}

void UCLCombatHudWidget::PinToGameViewport()
{
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetPositionInViewport(FVector2D::ZeroVector, false);

	const FVector2D Size = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).GetLocalSize();
	if (Size.X > 1.f && Size.Y > 1.f)
	{
		SetDesiredSizeInViewport(Size);
	}
}

FVector2D UCLCombatHudWidget::ResolveLayoutSize(const FGeometry* TickGeometry) const
{
	const FVector2D Slate = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).GetLocalSize();
	if (Slate.X > 1.f && Slate.Y > 1.f)
	{
		return Slate;
	}
	if (TickGeometry)
	{
		const FVector2D Local = TickGeometry->GetLocalSize();
		if (Local.X > 1.f && Local.Y > 1.f)
		{
			return Local;
		}
	}
	if (LastTickSize.X > 1.f && LastTickSize.Y > 1.f)
	{
		return LastTickSize;
	}
	return FVector2D(1280.f, 720.f);
}

void UCLCombatHudWidget::Refresh(float DeltaTime)
{
	PinToGameViewport();

	if (GConfig)
	{
		GConfig->GetBool(TEXT("/Script/Calling.CLHudSettings"), TEXT("bShowHudKeybinds"), bShowKeybinds, GGameIni);
	}
	UCLInputBindSubsystem* Table = nullptr;
	if (const UGameInstance* GI = GetGameInstance())
	{
		Table = GI->GetSubsystem<UCLInputBindSubsystem>();
	}
	for (int32 i = 0; i < SlotKeys.Num(); ++i)
	{
		if (!SlotKeys[i])
		{
			continue;
		}
		if (Table && i < HudSlotCount)
		{
			const FCLActionBinds Pair = Table->GetBinds(HudSlots[i].BindAction);
			const FString Caption = Pair.Primary.IsSet() ? Pair.Primary.ToDisplayString()
				: (Pair.Secondary.IsSet() ? Pair.Secondary.ToDisplayString() : TEXT("!"));
			SlotKeys[i]->SetText(FText::FromString(Caption));
		}
		SlotKeys[i]->SetVisibility(bShowKeybinds ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyLayout(ResolveLayoutSize(nullptr));
	RefreshFromPawn(DeltaTime);
}

void UCLCombatHudWidget::ApplyLayout(const FVector2D& ViewportSize)
{
	const float Short = FMath::Max(1.f, FMath::Min(ViewportSize.X, ViewportSize.Y));
	const float Inset = Short * 0.02f;
	const float Box = Short * 0.05f;
	const float Gap = Box * 0.14f;

	for (int32 i = 0; i < SlotBoxes.Num(); ++i)
	{
		if (SlotBoxes[i])
		{
			SlotBoxes[i]->SetWidthOverride(Box);
			SlotBoxes[i]->SetHeightOverride(Box);
		}
	}

	if (SlotRow)
	{
		for (int32 i = 0; i < SlotRow->GetChildrenCount(); ++i)
		{
			if (UWidget* Child = SlotRow->GetChildAt(i))
			{
				if (UHorizontalBoxSlot* RowSlot = Cast<UHorizontalBoxSlot>(Child->Slot))
				{
					const float Right = (i + 1 < SlotRow->GetChildrenCount()) ? Gap : 0.f;
					RowSlot->SetPadding(FMargin(0.f, 0.f, Right, 0.f));
				}
			}
		}
	}

	if (SlotRowSlot)
	{
		SlotRowSlot->SetPadding(FMargin(Inset, 0.f, 0.f, Inset));
	}
	const float RowW = SlotBoxes.Num() * Box + FMath::Max(0, SlotBoxes.Num() - 1) * Gap;
	if (Vitals)
	{
		Vitals->ApplyLayout(ViewportSize, Inset, RowW, Box);
	}
	if (GunSlot)
	{
		GunSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Inset));
	}
	if (ScoreSlot)
	{
		ScoreSlot->SetPadding(FMargin(0.f, Inset, 0.f, 0.f));
	}
}

void UCLCombatHudWidget::RefreshFromPawn(float DeltaTime)
{
	const ACLPlayerCharacter* Char = ResolveHudCharacter();
	if (Vitals)
	{
		Vitals->RefreshSelf(Char);
		Vitals->RefreshSighted(Char, DeltaTime);
	}
	if (Radar)
	{
		Radar->Refresh(Char, DeltaTime);
	}
	if (!Char)
	{
		return;
	}

	UCLAbilityLoadoutComponent* Loadout = Char->GetAbilities();
	UCLCombatMovementComponent* Move = Char->GetCombatMovement();
	for (int32 i = 0; i < SlotBorders.Num() && i < HudSlotCount; ++i)
	{
		float Remaining = 0.f;
		float Total = 1.f;
		bool bHasVerb = false;
		if (HudSlots[i].bDodge)
		{
			bHasVerb = Move != nullptr;
			Remaining = Move ? Move->GetDodgeCooldownRemaining() : 0.f;
			Total = Move ? FMath::Max(0.01f, Move->GetDodgeCooldownSeconds()) : 1.f;
		}
		else if (Loadout)
		{
			if (const UCLAbility* Ability = Loadout->GetSlot(HudSlots[i].AbilitySlot))
			{
				bHasVerb = true;
				Remaining = Ability->GetCooldownRemaining();
				Total = FMath::Max(0.01f, Ability->GetCooldown());
			}
		}

		const float Fill = bHasVerb ? FMath::Clamp(1.f - Remaining / Total, 0.f, 1.f) : 0.f;
		const bool bReady = bHasVerb && Remaining <= 0.f;
		if (SlotFills.IsValidIndex(i) && SlotFills[i])
		{
			SlotFills[i]->SetPercent(Fill);
			SlotFills[i]->SetFillColorAndOpacity(bReady
				? FLinearColor(0.28f, 0.32f, 0.36f, 0.95f)
				: FLinearColor(0.14f, 0.15f, 0.17f, 0.9f));
		}
		if (SlotGlyphs.IsValidIndex(i) && SlotGlyphs[i])
		{
			SlotGlyphs[i]->SetColorAndOpacity(FSlateColor(bReady
				? FLinearColor::White
				: FLinearColor(0.45f, 0.45f, 0.45f, 0.85f)));
		}
	}

	if (GunLine)
	{
		if (const UCLWeaponMotorComponent* Gun = Char->GetWeaponMotor())
		{
			const FString Name = Gun->GetActiveItem().DisplayName.IsEmpty()
				? TEXT("Unarmed")
				: Gun->GetActiveItem().DisplayName;
			if (Gun->IsSpecialEquipped())
			{
				GunLine->SetText(FText::FromString(FString::Printf(
					TEXT("%s  %d / %d"), *Name, Gun->GetAmmoInMag(), Gun->GetSpecialReserve())));
			}
			else
			{
				GunLine->SetText(FText::FromString(FString::Printf(
					TEXT("%s  %d"), *Name, Gun->GetAmmoInMag())));
			}
		}
	}

	if (ScoreLine)
	{
		FString Line;
		if (const UWorld* World = Char->GetWorld())
		{
			if (const ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
			{
				Line = GS->GetScoreLine();
			}
		}
		ScoreLine->SetText(FText::FromString(Line));
		ScoreLine->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

const ACLPlayerCharacter* UCLCombatHudWidget::ResolveHudCharacter() const
{
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const ACLPlayerCharacter* Viewed = Cast<ACLPlayerCharacter>(PC->GetViewTarget()))
		{
			return Viewed;
		}
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			if (const ACLPlayerCharacter* Demo = Cast<ACLPlayerCharacter>(Lobby->GetDemoViewPawn()))
			{
				return Demo;
			}
		}
	}
	if (const APlayerController* PC = GetOwningPlayer())
	{
		return Cast<ACLPlayerCharacter>(PC->GetPawn());
	}
	return nullptr;
}
