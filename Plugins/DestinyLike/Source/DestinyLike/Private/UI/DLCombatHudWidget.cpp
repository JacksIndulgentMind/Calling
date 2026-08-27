#include "UI/DLCombatHudWidget.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLAbilityLoadoutComponent.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Player/DLWeaponMotorComponent.h"
#include "Combat/DLDamageableComponent.h"
#include "Combat/DLHitscanService.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLLobbyTypes.h"
#include "Camera/CameraComponent.h"
#include "Ability/DLAbility.h"
#include "Ability/DLAbilityTypes.h"
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
#include "Game/DLInputBindSubsystem.h"
#include "Input/DLInputTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Layout/Anchors.h"

namespace
{
	struct FHudSlotDef
	{
		const TCHAR* Glyph;
		EDLAbilitySlot AbilitySlot;
		bool bDodge;
		EDLBindableAction BindAction;
	};

	const FHudSlotDef HudSlots[] = {
		{ TEXT("●"), EDLAbilitySlot::Grenade, false, EDLBindableAction::Grenade },
		{ TEXT("†"), EDLAbilitySlot::Melee, false, EDLBindableAction::Melee },
		{ TEXT("»"), EDLAbilitySlot::Dash, false, EDLBindableAction::Dash },
		{ TEXT("▣"), EDLAbilitySlot::Shield, false, EDLBindableAction::Shield },
		{ TEXT("◌"), EDLAbilitySlot::Evasion, false, EDLBindableAction::Evasion },
		{ TEXT("↷"), EDLAbilitySlot::Grenade, true, EDLBindableAction::Dodge },
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

TSharedRef<SWidget> UDLCombatHudWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UDLCombatHudWidget::BuildWidgetTree()
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
}

void UDLCombatHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	PinToGameViewport();
	Refresh();
}

int32 UDLCombatHudWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	PaintHurtVignette(AllottedGeometry, OutDrawElements, Layer + 1);
	PaintSightCrosshair(AllottedGeometry, OutDrawElements, Layer + 2);
	PaintRadar(AllottedGeometry, OutDrawElements, Layer + 3);
	return Layer + 4;
}

void UDLCombatHudWidget::PaintSightCrosshair(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const ADLPlayerCharacter* Char = ResolveHudCharacter();
	if (!Char || !Char->GetWeaponMotor() || Char->GetThirdPersonAlpha() > 0.55f)
	{
		return;
	}

	const UDLWeaponMotorComponent* Gun = Char->GetWeaponMotor();
	const FName Sight = Gun->GetSightId();
	const float Ads = Gun->GetAdsEase();
	const float Tighten = FMath::Lerp(1.f, 0.62f, Ads);
	FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
	if (Ads > 0.4f)
	{
		const FVector2D Punch = Char->GetAdsReticlePunch();
		const float PxPerDeg = AllottedGeometry.GetLocalSize().X / 90.f;
		Center += FVector2D(Punch.X, -Punch.Y) * PxPerDeg;
	}
	const FLinearColor Color(1.f, 1.f, 1.f, 0.92f);
	const FPaintGeometry Paint = AllottedGeometry.ToPaintGeometry();

	auto Line = [&](const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		Pts.Add(A);
		Pts.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	auto Ring = [&](float Radius, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		const int32 Segs = 28;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = (2.f * PI * i) / Segs;
			Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	if (Sight == FName(TEXT("iron")))
	{
		const float Arm = 11.f * Tighten;
		const float Gap = 3.f * Tighten;
		Line(Center + FVector2D(-Arm, Arm * 0.45f), Center + FVector2D(-Gap, Gap * 0.2f), 1.8f, Color);
		Line(Center + FVector2D(Arm, Arm * 0.45f), Center + FVector2D(Gap, Gap * 0.2f), 1.8f, Color);
		Line(Center + FVector2D(-Arm * 0.55f, Arm * 0.7f), Center + FVector2D(Arm * 0.55f, Arm * 0.7f), 1.6f, Color);
		return;
	}

	if (Sight == FName(TEXT("scope")))
	{
		if (Ads > 0.02f)
		{
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const float Outer = FMath::Min(Size.X, Size.Y) * 0.48f;
			const float Inner = Outer * 0.36f;
			const FLinearColor Rim(0.02f, 0.02f, 0.025f, 0.92f * Ads);
			for (float R = Inner; R <= Outer; R += 3.5f)
			{
				Ring(R, 3.2f, Rim);
			}
			static const FSlateColorBrush DarkBrush(FLinearColor::White);
			const FLinearColor Vignette(0.01f, 0.01f, 0.015f, 0.72f * Ads);
			auto Box = [&](const FVector2D& Pos, const FVector2D& BoxSize)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(1.f, FVector2f(Pos))),
					&DarkBrush,
					ESlateDrawEffect::None,
					Vignette);
			};
			const float Hole = Inner * 1.05f;
			Box(FVector2D(0.f, 0.f), FVector2D(Size.X, FMath::Max(0.f, Center.Y - Hole)));
			Box(FVector2D(0.f, Center.Y + Hole), FVector2D(Size.X, FMath::Max(0.f, Size.Y - (Center.Y + Hole))));
			Box(FVector2D(0.f, Center.Y - Hole), FVector2D(FMath::Max(0.f, Center.X - Hole), Hole * 2.f));
			Box(FVector2D(Center.X + Hole, Center.Y - Hole), FVector2D(FMath::Max(0.f, Size.X - (Center.X + Hole)), Hole * 2.f));
			const float Gap = 4.f;
			Line(Center + FVector2D(-18.f, 0.f), Center + FVector2D(-Gap, 0.f), 1.1f, Color);
			Line(Center + FVector2D(Gap, 0.f), Center + FVector2D(18.f, 0.f), 1.1f, Color);
			Line(Center + FVector2D(0.f, -18.f), Center + FVector2D(0.f, -Gap), 1.1f, Color);
			Line(Center + FVector2D(0.f, Gap), Center + FVector2D(0.f, 18.f), 1.1f, Color);
		}
		return;
	}

	if (Ads < 0.45f)
	{
		const float Radius = 6.f * Tighten;
		Ring(Radius, 1.2f, FLinearColor(1.f, 1.f, 1.f, 0.45f));
		const float Pip = 1.2f;
		Line(Center + FVector2D(-Pip, 0.f), Center + FVector2D(Pip, 0.f), 1.2f, Color);
		Line(Center + FVector2D(0.f, -Pip), Center + FVector2D(0.f, Pip), 1.2f, Color);
		return;
	}

	const FLinearColor Led(1.f, 0.12f, 0.08f, 0.95f);
	const float Pip = 1.6f;
	Line(Center + FVector2D(-Pip, 0.f), Center + FVector2D(Pip, 0.f), 1.8f, Led);
	Line(Center + FVector2D(0.f, -Pip), Center + FVector2D(0.f, Pip), 1.8f, Led);
}

void UDLCombatHudWidget::PaintHurtVignette(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const ADLPlayerCharacter* Char = ResolveHudCharacter();
	if (!Char)
	{
		return;
	}
	const float Alpha = Char->GetHurtAlpha();
	if (Alpha <= 0.01f)
	{
		return;
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float EdgeX = FMath::Max(48.f, Size.X * 0.11f);
	const float EdgeY = FMath::Max(36.f, Size.Y * 0.14f);
	const FLinearColor Col(0.62f, 0.02f, 0.05f, 0.58f * Alpha);
	static const FSlateColorBrush Brush(FLinearColor::White);

	auto Box = [&](const FVector2D& Pos, const FVector2D& BoxSize)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(1.f, FVector2f(Pos))),
			&Brush,
			ESlateDrawEffect::None,
			Col);
	};

	Box(FVector2D(0.f, 0.f), FVector2D(Size.X, EdgeY));
	Box(FVector2D(0.f, Size.Y - EdgeY), FVector2D(Size.X, EdgeY));
	Box(FVector2D(0.f, EdgeY), FVector2D(EdgeX, Size.Y - EdgeY * 2.f));
	Box(FVector2D(Size.X - EdgeX, EdgeY), FVector2D(EdgeX, Size.Y - EdgeY * 2.f));
}

void UDLCombatHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LastTickSize = MyGeometry.GetLocalSize();
	Refresh(InDeltaTime);
}

void UDLCombatHudWidget::PinToGameViewport()
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

FVector2D UDLCombatHudWidget::ResolveLayoutSize(const FGeometry* TickGeometry) const
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

void UDLCombatHudWidget::Refresh(float DeltaTime)
{
	PinToGameViewport();

	if (GConfig)
	{
		GConfig->GetBool(TEXT("/Script/DestinyLike.DLHudSettings"), TEXT("bShowHudKeybinds"), bShowKeybinds, GGameIni);
	}
	UDLInputBindSubsystem* Table = nullptr;
	if (const UGameInstance* GI = GetGameInstance())
	{
		Table = GI->GetSubsystem<UDLInputBindSubsystem>();
	}
	for (int32 i = 0; i < SlotKeys.Num(); ++i)
	{
		if (!SlotKeys[i])
		{
			continue;
		}
		if (Table && i < HudSlotCount)
		{
			const FDLActionBinds Pair = Table->GetBinds(HudSlots[i].BindAction);
			const FString Caption = Pair.Primary.IsSet() ? Pair.Primary.ToDisplayString()
				: (Pair.Secondary.IsSet() ? Pair.Secondary.ToDisplayString() : TEXT("!"));
			SlotKeys[i]->SetText(FText::FromString(Caption));
		}
		SlotKeys[i]->SetVisibility(bShowKeybinds ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	ApplyLayout(ResolveLayoutSize(nullptr));
	RefreshFromPawn(DeltaTime);
}

void UDLCombatHudWidget::ApplyLayout(const FVector2D& ViewportSize)
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
	const float SelfBarH = FMath::Max(5.f, Box * 0.14f);
	if (SelfVitalsBox)
	{
		SelfVitalsBox->SetWidthOverride(RowW);
		SelfVitalsBox->SetHeightOverride(SelfBarH * 2.f + 3.f);
	}
	if (SelfShieldBox)
	{
		SelfShieldBox->SetHeightOverride(SelfBarH);
	}
	if (SelfHealthBox)
	{
		SelfHealthBox->SetHeightOverride(SelfBarH);
	}
	const float SightW = Short * 0.16f;
	const float SightBarH = SelfBarH;
	if (SightedBox)
	{
		SightedBox->SetWidthOverride(SightW);
		SightedBox->SetHeightOverride(SightBarH * 2.f + 2.f);
	}
	if (SightedShieldBox)
	{
		SightedShieldBox->SetHeightOverride(SightBarH);
	}
	if (SightedHealthBox)
	{
		SightedHealthBox->SetHeightOverride(SightBarH);
	}
	if (SightedSlot)
	{
		SightedSlot->SetPadding(FMargin(0.f, 0.f, Inset, Inset));
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

void UDLCombatHudWidget::RefreshFromPawn(float DeltaTime)
{
	const ADLPlayerCharacter* Char = ResolveHudCharacter();
	RefreshSelfVitals(Char);
	RefreshSightedTarget(Char, DeltaTime);
	RefreshRadar(Char, DeltaTime);
	if (!Char)
	{
		return;
	}

	UDLAbilityLoadoutComponent* Loadout = Char->GetAbilities();
	UDLCombatMovementComponent* Move = Char->GetCombatMovement();
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
			if (const UDLAbility* Ability = Loadout->GetSlot(HudSlots[i].AbilitySlot))
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
		if (const UDLWeaponMotorComponent* Gun = Char->GetWeaponMotor())
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
			if (const ADLGameStateBase* GS = World->GetGameState<ADLGameStateBase>())
			{
				Line = GS->GetScoreLine();
			}
		}
		ScoreLine->SetText(FText::FromString(Line));
		ScoreLine->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

const ADLPlayerCharacter* UDLCombatHudWidget::ResolveHudCharacter() const
{
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const ADLPlayerCharacter* Viewed = Cast<ADLPlayerCharacter>(PC->GetViewTarget()))
		{
			return Viewed;
		}
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (const ADLPlayerCharacter* Demo = Cast<ADLPlayerCharacter>(Lobby->GetDemoViewPawn()))
			{
				return Demo;
			}
		}
	}
	if (const APlayerController* PC = GetOwningPlayer())
	{
		return Cast<ADLPlayerCharacter>(PC->GetPawn());
	}
	return nullptr;
}

void UDLCombatHudWidget::RefreshSelfVitals(const ADLPlayerCharacter* Char)
{
	const UDLHealthShieldComponent* HS = Char ? Char->GetHealthShield() : nullptr;
	if (SelfVitalsBox)
	{
		SelfVitalsBox->SetVisibility(HS ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!HS)
	{
		return;
	}
	const float MaxS = FMath::Max(1.f, HS->GetTune().MaxShield);
	const float MaxH = FMath::Max(1.f, HS->GetTune().MaxHealth);
	if (SelfShieldBar)
	{
		SelfShieldBar->SetPercent(FMath::Clamp(HS->GetShield() / MaxS, 0.f, 1.f));
	}
	if (SelfHealthBar)
	{
		SelfHealthBar->SetPercent(FMath::Clamp(HS->GetHealth() / MaxH, 0.f, 1.f));
	}
}

void UDLCombatHudWidget::RefreshSightedTarget(const ADLPlayerCharacter* Viewer, float DeltaTime)
{
	constexpr float FadeIn = 0.18f;
	constexpr float FadeOut = 0.40f;
	FDLSightedTarget Target;
	const bool bHit = DLHitscanService::QuerySightedFromPawn(Viewer, Target);
	if (bHit)
	{
		if (SightedActor.Get() != Target.Actor.Get())
		{
			SightedSeenAge = 0.f;
		}
		SightedActor = Target.Actor;
		SightedHealth = Target.Health;
		SightedShield = Target.Shield;
		SightedMaxHealth = Target.MaxHealth;
		SightedMaxShield = Target.MaxShield;
		SightedLostAge = 0.f;
		SightedSeenAge += DeltaTime;
		SightedAlpha = FMath::Clamp(SightedSeenAge / FadeIn, 0.f, 1.f);
	}
	else
	{
		SightedSeenAge = 0.f;
		SightedLostAge += DeltaTime;
		SightedAlpha = 1.f - FMath::Clamp(SightedLostAge / FadeOut, 0.f, 1.f);
		if (SightedLostAge >= FadeOut)
		{
			SightedActor.Reset();
			SightedAlpha = 0.f;
		}
	}

	if (!SightedBox)
	{
		return;
	}
	if (SightedAlpha <= 0.01f)
	{
		SightedBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SightedBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	SightedBox->SetRenderOpacity(SightedAlpha);
	if (SightedHealthBar)
	{
		SightedHealthBar->SetPercent(FMath::Clamp(SightedHealth / FMath::Max(1.f, SightedMaxHealth), 0.f, 1.f));
	}
	const bool bHasShield = SightedMaxShield > 0.5f;
	if (SightedShieldBox)
	{
		SightedShieldBox->SetVisibility(bHasShield ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SightedShieldBar && bHasShield)
	{
		SightedShieldBar->SetPercent(FMath::Clamp(SightedShield / FMath::Max(1.f, SightedMaxShield), 0.f, 1.f));
	}
}

void UDLCombatHudWidget::RefreshRadar(const ADLPlayerCharacter* Viewer, float DeltaTime)
{
	RadarBlips.Reset();
	if (!Viewer)
	{
		RadarTracks.Reset();
		RadarScatterClock = 0.f;
		RadarWedge[0] = RadarWedge[1] = RadarWedge[2] = 0.f;
		return;
	}

	constexpr float FadeIn = 0.18f;
	constexpr float RippleDecay = 0.22f;
	constexpr float RangeCm = 3500.f;
	constexpr int32 TrailMax = 6;
	TArray<FDLRadarContact> Contacts;
	DLHitscanService::QueryRadarContacts(Viewer, Contacts, RangeCm, true);

	RadarTracks.RemoveAll([&](const FDLRadarTrack& Track)
	{
		if (!Track.Actor.IsValid())
		{
			return true;
		}
		for (const FDLRadarContact& Contact : Contacts)
		{
			if (Contact.Actor.Get() == Track.Actor.Get())
			{
				return false;
			}
		}
		return true;
	});

	auto FindTrack = [this](const AActor* Actor) -> FDLRadarTrack*
	{
		for (FDLRadarTrack& Track : RadarTracks)
		{
			if (Track.Actor.Get() == Actor)
			{
				return &Track;
			}
		}
		return nullptr;
	};

	auto Resample = [](FDLRadarTrack& Track, const FDLRadarContact& Contact)
	{
		const float DistT = FMath::Clamp(Contact.DistXY / RangeCm, 0.f, 1.f);
		const float MaxAng = Contact.bLowProfile ? FMath::Lerp(36.f, 99.f, DistT) : FMath::Lerp(4.5f, 24.f, DistT);
		const float RadialFrac = Contact.bLowProfile ? FMath::Lerp(0.12f, 0.54f, DistT) : FMath::Lerp(0.045f, 0.18f, DistT);
		Track.ScatterYawDeg = FMath::FRandRange(-MaxAng, MaxAng);
		Track.ScatterRadial = Contact.DistXY * RadialFrac * FMath::FRandRange(-1.f, 1.f);
	};

	RadarScatterClock += DeltaTime;
	const bool bResample = RadarScatterClock >= (1.f / 15.f);
	if (bResample)
	{
		RadarScatterClock = 0.f;
	}

	for (const FDLRadarContact& Contact : Contacts)
	{
		AActor* Actor = Contact.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		FDLRadarTrack* Track = FindTrack(Actor);
		const bool bNew = Track == nullptr;
		if (bNew)
		{
			FDLRadarTrack Fresh;
			Fresh.Actor = Actor;
			Fresh.Alpha = 0.f;
			Resample(Fresh, Contact);
			RadarTracks.Add(Fresh);
			Track = &RadarTracks.Last();
		}
		Track->Alpha = FMath::Clamp(Track->Alpha + DeltaTime / FadeIn, 0.f, 1.f);
		if (bResample && !bNew)
		{
			if (Track->bHasOffset)
			{
				Track->Trail.Add(Track->LastOffset);
				while (Track->Trail.Num() > TrailMax)
				{
					Track->Trail.RemoveAt(0);
				}
			}
			Resample(*Track, Contact);
		}
	}

	FVector Eye = Viewer->GetActorLocation();
	FVector Fwd = Viewer->GetActorForwardVector();
	if (const UCameraComponent* Cam = Viewer->GetFollowCamera())
	{
		Eye = Cam->GetComponentLocation();
		Fwd = Cam->GetForwardVector();
	}
	Fwd.Z = 0.f;
	if (!Fwd.Normalize())
	{
		Fwd = FVector::ForwardVector;
	}
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);

	const UDLLobbySubsystem* Lobby = nullptr;
	if (const UGameInstance* GI = GetGameInstance())
	{
		Lobby = GI->GetSubsystem<UDLLobbySubsystem>();
	}

	for (const FDLRadarContact& Contact : Contacts)
	{
		FDLRadarTrack* Track = FindTrack(Contact.Actor.Get());
		if (!Track)
		{
			continue;
		}
		const float Forward = (Contact.Location.X - Eye.X) * Fwd.X + (Contact.Location.Y - Eye.Y) * Fwd.Y;
		const float RightAmt = (Contact.Location.X - Eye.X) * Right.X + (Contact.Location.Y - Eye.Y) * Right.Y;
		const float TrueAng = FMath::Atan2(RightAmt, Forward);
		const float TrueR = FVector::Dist2D(Contact.Location, Eye);
		const float PaintAng = TrueAng + FMath::DegreesToRadians(Track->ScatterYawDeg);
		const float PaintR = FMath::Clamp(TrueR + Track->ScatterRadial, 0.f, RangeCm);
		const float U = (PaintR / RangeCm) * FMath::Sin(PaintAng);
		const float V = (PaintR / RangeCm) * FMath::Cos(PaintAng);
		const FVector2D Offset(U, -V);
		Track->LastOffset = Offset;
		Track->bHasOffset = true;

		EDLPvpTeam Team = EDLPvpTeam::Unassigned;
		if (Lobby)
		{
			for (const UDLParticipantSeat* Seat : Lobby->GetSeats())
			{
				if (Seat && Seat->GetDrivenPawn() == Contact.Actor.Get())
				{
					Team = Seat->GetTeam();
					break;
				}
			}
		}
		FLinearColor Col(0.88f, 0.86f, 0.72f, 1.f);
		if (Team == EDLPvpTeam::Red)
		{
			Col = FLinearColor(0.92f, 0.16f, 0.12f, 1.f);
		}
		else if (Team == EDLPvpTeam::Blue)
		{
			Col = FLinearColor(0.18f, 0.48f, 1.f, 1.f);
		}

		const float DistT = FMath::Clamp(Contact.DistXY / RangeCm, 0.f, 1.f);
		FDLRadarPaintBlip Blip;
		Blip.Offset = Offset;
		Blip.Trail = Track->Trail;
		Blip.Color = Col;
		Blip.Alpha = Track->Alpha * FMath::Lerp(1.f, 0.22f, DistT) * (Contact.bLowProfile ? 0.55f : 1.f);
		Blip.Size = FMath::Lerp(11.f, 5.2f, DistT);
		RadarBlips.Add(Blip);
	}

	TArray<FDLRadarContact> Footsteps;
	DLHitscanService::QueryRadarContacts(Viewer, Footsteps, RangeCm, false);
	bool bWedgeOn[3] = {};
	for (const FDLRadarContact& Contact : Footsteps)
	{
		const float Forward = (Contact.Location.X - Eye.X) * Fwd.X + (Contact.Location.Y - Eye.Y) * Fwd.Y;
		const float RightAmt = (Contact.Location.X - Eye.X) * Right.X + (Contact.Location.Y - Eye.Y) * Right.Y;
		const float Deg = FMath::RadiansToDegrees(FMath::Atan2(RightAmt, Forward));
		int32 Wedge = 2;
		if (Deg >= -60.f && Deg < 60.f)
		{
			Wedge = 0;
		}
		else if (Deg >= 60.f)
		{
			Wedge = 1;
		}
		bWedgeOn[Wedge] = true;
	}
	for (int32 i = 0; i < 3; ++i)
	{
		if (bWedgeOn[i])
		{
			RadarWedge[i] = FMath::Clamp(RadarWedge[i] + DeltaTime / FadeIn, 0.f, 1.f);
		}
		else
		{
			RadarWedge[i] = FMath::Max(0.f, RadarWedge[i] - DeltaTime / RippleDecay);
		}
	}
}

void UDLCombatHudWidget::PaintRadar(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (!ResolveHudCharacter())
	{
		return;
	}
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Short = FMath::Max(1.f, FMath::Min(Size.X, Size.Y));
	const float Dia = Short * 0.16f;
	const float Inset = Short * 0.02f;
	const FVector2D Center(Inset + Dia * 0.5f, Inset + Dia * 0.5f);
	const float Radius = Dia * 0.5f;
	const FPaintGeometry Paint = AllottedGeometry.ToPaintGeometry();
	const FLinearColor RingCol(1.f, 1.f, 1.f, 0.28f);
	const FLinearColor TickCol(1.f, 1.f, 1.f, 0.45f);
	const FLinearColor InnerFill(0.06f, 0.06f, 0.07f, 0.78f);
	const FLinearColor RippleCol(0.82f, 0.88f, 0.95f, 1.f);

	auto Ring = [&](float Rad, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		const int32 Segs = 32;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float A = (2.f * PI * i) / Segs;
			Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rad);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	auto Line = [&](const FVector2D& A, const FVector2D& B, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		Pts.Add(A);
		Pts.Add(B);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	auto Arc = [&](float Rad, float Deg0, float Deg1, float Thickness, const FLinearColor& Col)
	{
		TArray<FVector2D> Pts;
		const int32 Segs = 12;
		for (int32 i = 0; i <= Segs; ++i)
		{
			const float Deg = FMath::Lerp(Deg0, Deg1, i / static_cast<float>(Segs));
			const float A = FMath::DegreesToRadians(Deg);
			Pts.Add(Center + FVector2D(FMath::Sin(A), -FMath::Cos(A)) * Rad);
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Paint, Pts, ESlateDrawEffect::None, Col, false, Thickness);
	};

	for (int32 i = 1; i <= 8; ++i)
	{
		const float T = i / 8.f;
		Ring(Radius * 0.5f * T, FMath::Max(2.2f, Radius * 0.08f), InnerFill);
	}
	Ring(Radius, 6.f, FLinearColor(1.f, 1.f, 1.f, 0.12f));
	Ring(Radius, 3.2f, RingCol);

	static const float WedgeDeg[3][2] = { { -60.f, 60.f }, { 60.f, 180.f }, { -180.f, -60.f } };
	for (int32 w = 0; w < 3; ++w)
	{
		if (RadarWedge[w] < 0.02f)
		{
			continue;
		}
		for (int32 k = 0; k < 8; ++k)
		{
			const float RimT = (k + 1) / 8.f;
			FLinearColor Col = RippleCol;
			Col.A = RadarWedge[w] * FMath::Lerp(0.04f, 0.32f, RimT);
			Arc(Radius * RimT, WedgeDeg[w][0], WedgeDeg[w][1], FMath::Lerp(1.6f, 3.4f, RimT), Col);
		}
	}

	Line(Center + FVector2D(0.f, -Radius * 0.82f), Center + FVector2D(0.f, -Radius), 2.2f, TickCol);
	Line(Center + FVector2D(-2.2f, 0.f), Center + FVector2D(2.2f, 0.f), 1.2f, TickCol);
	Line(Center + FVector2D(0.f, -2.2f), Center + FVector2D(0.f, 2.2f), 1.2f, TickCol);

	static const FSlateColorBrush Brush(FLinearColor::White);
	auto BlipBox = [&](const FVector2D& Pos, float S, const FLinearColor& Col)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(S, S), FSlateLayoutTransform(1.f, FVector2f(Pos - FVector2D(S * 0.5f, S * 0.5f)))),
			&Brush,
			ESlateDrawEffect::None,
			Col);
	};
	auto BloomBlip = [&](const FVector2D& Pos, float S, const FLinearColor& Base, float Alpha)
	{
		FLinearColor Halo = Base;
		Halo.A = Alpha * 0.12f;
		BlipBox(Pos, S * 2.4f, Halo);
		FLinearColor Mid = Base;
		Mid.A = Alpha * 0.28f;
		BlipBox(Pos, S * 1.6f, Mid);
		FLinearColor Core = Base;
		Core.A = Alpha;
		BlipBox(Pos, S, Core);
	};

	for (const FDLRadarPaintBlip& Blip : RadarBlips)
	{
		if (Blip.Alpha < 0.02f)
		{
			continue;
		}
		for (int32 i = 0; i < Blip.Trail.Num(); ++i)
		{
			const float TrailA = Blip.Alpha * (i + 1) / 6.f;
			if (TrailA < 0.02f)
			{
				continue;
			}
			const FVector2D Pos = Center + Blip.Trail[i] * Radius;
			BloomBlip(Pos, Blip.Size * 0.72f, Blip.Color, TrailA);
		}
		BloomBlip(Center + Blip.Offset * Radius, Blip.Size, Blip.Color, Blip.Alpha);
	}
}
