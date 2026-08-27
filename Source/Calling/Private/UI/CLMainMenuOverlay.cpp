#include "UI/CLMainMenuOverlay.h"
#include "UI/CLDirectorPanel.h"
#include "UI/CLKeybindEditor.h"
#include "Game/CLErrorBoundary.h"
#include "Core/CLError.h"
#include "Game/CLSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Input/CLInputTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName& Name, const FString& Text, int32 FontSize = 14)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return Label;
	}

	void StyleButtonLabel(UButton* Button, UTextBlock* Label)
	{
		Button->AddChild(Label);
		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(Label->Slot))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Center);
			BtnSlot->SetVerticalAlignment(VAlign_Center);
			BtnSlot->SetPadding(FMargin(8.f, 4.f));
		}
	}

	UButton* MakeTextButton(UWidgetTree* Tree, const FName& Name, const FString& Text)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *Name.ToString()));
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 14;
		Label->SetFont(Font);
		StyleButtonLabel(Button, Label);
		return Button;
	}

	void AddPadded(UVerticalBox* Box, UWidget* Child, float Bottom = 8.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

UCLMainMenuOverlay::UCLMainMenuOverlay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DirectorPanel = CreateDefaultSubobject<UCLDirectorPanel>(TEXT("DirectorPanel"));
	KeybindEditor = CreateDefaultSubobject<UCLKeybindEditor>(TEXT("KeybindEditor"));
}

TSharedRef<SWidget> UCLMainMenuOverlay::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UCLMainMenuOverlay::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
	bVisible = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->OnSessionEvent.AddDynamic(this, &UCLMainMenuOverlay::HandleSessionEvent);
		}
	}
}

void UCLMainMenuOverlay::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->OnSessionEvent.RemoveDynamic(this, &UCLMainMenuOverlay::HandleSessionEvent);
		}
	}
	Super::NativeDestruct();
}

void UCLMainMenuOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (KeybindEditor)
	{
		KeybindEditor->TickListen();
	}
}

void UCLMainMenuOverlay::HandleSessionEvent(bool bSuccess, const FString& Message)
{
	if (bSuccess)
	{
		return;
	}
	UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("session"), Message));
}

void UCLMainMenuOverlay::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.72f));
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelSize"));
	PanelSize->SetWidthOverride(720.f);
	PanelSize->SetHeightOverride(620.f);
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelSize))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.06f, 0.07f, 0.1f, 0.96f));
	Panel->SetPadding(FMargin(24.f, 20.f));
	PanelSize->AddChild(Panel);

	UVerticalBox* RootCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootCol"));
	Panel->AddChild(RootCol);

	AddPadded(RootCol, MakeLabel(WidgetTree, TEXT("Title"), TEXT("Director"), 26), 12.f);

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Tabs"));
	UButton* DirectorTab = MakeTextButton(WidgetTree, TEXT("DirectorTab"), TEXT("Director"));
	DirectorTab->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleDirectorTabClicked);
	UButton* KeybindsTab = MakeTextButton(WidgetTree, TEXT("KeybindsTab"), TEXT("Keybinds"));
	KeybindsTab->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleKeybindsTabClicked);
	if (UHorizontalBoxSlot* T0 = Tabs->AddChildToHorizontalBox(DirectorTab))
	{
		T0->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	Tabs->AddChildToHorizontalBox(KeybindsTab);
	AddPadded(RootCol, Tabs, 16.f);

	BuildDirectorPanel(RootCol);
	BuildKeybindEditor(RootCol);
}

void UCLMainMenuOverlay::BuildDirectorPanel(UVerticalBox* RootCol)
{
	DirectorBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DirectorBox"));
	AddPadded(DirectorBox, MakeLabel(WidgetTree, TEXT("DirectorHint"), TEXT("Compose PvP opens the composer menu. Host or Guest, then Ready. Host Start launches PvP. Remote seats Ready/Start on the hub. Launch PvP is a solo arena skip."), 13), 12.f);

	UButton* ComposeBtn = MakeTextButton(WidgetTree, TEXT("ComposeBtn"), TEXT("Compose PvP"));
	ComposeBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleComposeClicked);
	AddPadded(DirectorBox, ComposeBtn);

	UButton* PvpBtn = MakeTextButton(WidgetTree, TEXT("PvpBtn"), TEXT("Launch PvP (skip)"));
	PvpBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandlePvpClicked);
	AddPadded(DirectorBox, PvpBtn);

	UButton* RaidBtn = MakeTextButton(WidgetTree, TEXT("RaidBtn"), TEXT("Launch Raid"));
	RaidBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleRaidClicked);
	AddPadded(DirectorBox, RaidBtn);

	UButton* PracticeBtn = MakeTextButton(WidgetTree, TEXT("PracticeBtn"), TEXT("Launch Practice"));
	PracticeBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandlePracticeClicked);
	AddPadded(DirectorBox, PracticeBtn);

	UButton* ReadyBtn = MakeTextButton(WidgetTree, TEXT("ReadyBtn"), TEXT("Ready (toggle)"));
	ReadyBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleReadyClicked);
	AddPadded(DirectorBox, ReadyBtn);

	UButton* RedBtn = MakeTextButton(WidgetTree, TEXT("JoinRedBtn"), TEXT("Join Red"));
	RedBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleJoinRedClicked);
	AddPadded(DirectorBox, RedBtn);

	UButton* BlueBtn = MakeTextButton(WidgetTree, TEXT("JoinBlueBtn"), TEXT("Join Blue"));
	BlueBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleJoinBlueClicked);
	AddPadded(DirectorBox, BlueBtn);

	UButton* GoBtn = MakeTextButton(WidgetTree, TEXT("GoBtn"), TEXT("Host Go"));
	GoBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleGoClicked);
	AddPadded(DirectorBox, GoBtn);

	UButton* HostOpenBtn = MakeTextButton(WidgetTree, TEXT("HostOpenBtn"), TEXT("Host Social (open)"));
	HostOpenBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleHostSocialOpenClicked);
	AddPadded(DirectorBox, HostOpenBtn);

	UButton* HostClosedBtn = MakeTextButton(WidgetTree, TEXT("HostClosedBtn"), TEXT("Host Social (closed)"));
	HostClosedBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleHostSocialClosedClicked);
	AddPadded(DirectorBox, HostClosedBtn);

	UButton* SocialBtn = MakeTextButton(WidgetTree, TEXT("SocialBtn"), TEXT("Exit to Social"));
	SocialBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleExitSocialClicked);
	AddPadded(DirectorBox, SocialBtn, 0.f);
	AddPadded(RootCol, DirectorBox, 0.f);
}

void UCLMainMenuOverlay::BuildKeybindEditor(UVerticalBox* RootCol)
{
	KeybindsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("KeybindsBox"));
	KeybindsBox->SetVisibility(ESlateVisibility::Collapsed);
	AddPadded(KeybindsBox, MakeLabel(WidgetTree, TEXT("BindHint"),
		TEXT("Click a bind to change it. Clear leaves it empty. Alt + key is a combo. Alt alone cannot be bound. Gamepad is its own column."), 12), 8.f);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BindHeader"));
	auto AddHeader = [&](const TCHAR* Name, const TCHAR* Text, float Fill)
	{
		UTextBlock* H = MakeLabel(WidgetTree, Name, Text, 12);
		if (UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(H))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Fill;
			Slot->SetSize(Size);
		}
	};
	AddHeader(TEXT("HAction"), TEXT("Action"), 1.2f);
	AddHeader(TEXT("HPrim"), TEXT("Primary"), 1.3f);
	AddHeader(TEXT("HPrimC"), TEXT(""), 0.5f);
	AddHeader(TEXT("HSec"), TEXT("Secondary"), 1.3f);
	AddHeader(TEXT("HSecC"), TEXT(""), 0.5f);
	AddHeader(TEXT("HPad"), TEXT("Gamepad"), 1.3f);
	AddHeader(TEXT("HPadC"), TEXT(""), 0.5f);
	AddPadded(KeybindsBox, Header, 6.f);

	UScrollBox* BindScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BindScroll"));
	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScrollSize"));
	ScrollSize->SetHeightOverride(360.f);
	ScrollSize->AddChild(BindScroll);

	const TArray<ECLBindableAction>& Actions = CLInput::AllActions();
	TArray<TObjectPtr<UTextBlock>> BindLabels;
	TArray<TObjectPtr<UTextBlock>> PrimaryLabels;
	TArray<TObjectPtr<UTextBlock>> SecondaryLabels;
	TArray<TObjectPtr<UTextBlock>> GamepadLabels;
	BindLabels.SetNum(Actions.Num());
	PrimaryLabels.SetNum(Actions.Num());
	SecondaryLabels.SetNum(Actions.Num());
	GamepadLabels.SetNum(Actions.Num());

	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		const ECLBindableAction Action = Actions[i];
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("BindRow%d"), i));

		BindLabels[i] = MakeLabel(WidgetTree, *FString::Printf(TEXT("BindName%d"), i), CLInput::ActionDisplayName(Action), 13);
		if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(BindLabels[i]))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.2f;
			NameSlot->SetSize(Size);
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		UCLBindSlotButton* PrimBtn = WidgetTree->ConstructWidget<UCLBindSlotButton>(UCLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PrimBtn%d"), i));
		PrimaryLabels[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PrimLbl%d"), i));
		PrimaryLabels[i]->SetText(FText::FromString(TEXT("â€”")));
		StyleButtonLabel(PrimBtn, PrimaryLabels[i]);
		PrimBtn->Configure(KeybindEditor, Action, ECLBindColumn::Primary, false);
		if (UHorizontalBoxSlot* PSlot = Row->AddChildToHorizontalBox(PrimBtn))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.3f;
			PSlot->SetSize(Size);
			PSlot->SetPadding(FMargin(0.f, 2.f, 4.f, 2.f));
		}

		UCLBindSlotButton* PrimClear = WidgetTree->ConstructWidget<UCLBindSlotButton>(UCLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PrimClr%d"), i));
		UTextBlock* PrimClearLbl = MakeLabel(WidgetTree, *FString::Printf(TEXT("PrimClrLbl%d"), i), TEXT("Clear"), 12);
		StyleButtonLabel(PrimClear, PrimClearLbl);
		PrimClear->Configure(KeybindEditor, Action, ECLBindColumn::Primary, true);
		if (UHorizontalBoxSlot* PCSlot = Row->AddChildToHorizontalBox(PrimClear))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 0.5f;
			PCSlot->SetSize(Size);
			PCSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
		}

		UCLBindSlotButton* SecBtn = WidgetTree->ConstructWidget<UCLBindSlotButton>(UCLBindSlotButton::StaticClass(), *FString::Printf(TEXT("SecBtn%d"), i));
		SecondaryLabels[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SecLbl%d"), i));
		SecondaryLabels[i]->SetText(FText::FromString(TEXT("â€”")));
		StyleButtonLabel(SecBtn, SecondaryLabels[i]);
		SecBtn->Configure(KeybindEditor, Action, ECLBindColumn::Secondary, false);
		if (UHorizontalBoxSlot* SSlot = Row->AddChildToHorizontalBox(SecBtn))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.3f;
			SSlot->SetSize(Size);
			SSlot->SetPadding(FMargin(0.f, 2.f, 4.f, 2.f));
		}

		UCLBindSlotButton* SecClear = WidgetTree->ConstructWidget<UCLBindSlotButton>(UCLBindSlotButton::StaticClass(), *FString::Printf(TEXT("SecClr%d"), i));
		UTextBlock* SecClearLbl = MakeLabel(WidgetTree, *FString::Printf(TEXT("SecClrLbl%d"), i), TEXT("Clear"), 12);
		StyleButtonLabel(SecClear, SecClearLbl);
		SecClear->Configure(KeybindEditor, Action, ECLBindColumn::Secondary, true);
		if (UHorizontalBoxSlot* SCSlot = Row->AddChildToHorizontalBox(SecClear))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 0.5f;
			SCSlot->SetSize(Size);
			SCSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
		}

		UCLBindSlotButton* PadBtn = WidgetTree->ConstructWidget<UCLBindSlotButton>(UCLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PadBtn%d"), i));
		GamepadLabels[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PadLbl%d"), i));
		GamepadLabels[i]->SetText(FText::FromString(TEXT("â€”")));
		StyleButtonLabel(PadBtn, GamepadLabels[i]);
		PadBtn->Configure(KeybindEditor, Action, ECLBindColumn::Gamepad, false);
		if (UHorizontalBoxSlot* GSlot = Row->AddChildToHorizontalBox(PadBtn))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.3f;
			GSlot->SetSize(Size);
			GSlot->SetPadding(FMargin(0.f, 2.f, 4.f, 2.f));
		}

		UCLBindSlotButton* PadClear = WidgetTree->ConstructWidget<UCLBindSlotButton>(UCLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PadClr%d"), i));
		UTextBlock* PadClearLbl = MakeLabel(WidgetTree, *FString::Printf(TEXT("PadClrLbl%d"), i), TEXT("Clear"), 12);
		StyleButtonLabel(PadClear, PadClearLbl);
		PadClear->Configure(KeybindEditor, Action, ECLBindColumn::Gamepad, true);
		if (UHorizontalBoxSlot* GCSlot = Row->AddChildToHorizontalBox(PadClear))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 0.5f;
			GCSlot->SetSize(Size);
			GCSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
		}

		BindScroll->AddChild(Row);
	}

	AddPadded(KeybindsBox, ScrollSize, 8.f);

	UHorizontalBox* BindFooter = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BindFooter"));
	UButton* ResetBtn = MakeTextButton(WidgetTree, TEXT("ResetBtn"), TEXT("Reset all to defaults"));
	ResetBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleResetDefaultsClicked);
	UButton* DoneBtn = MakeTextButton(WidgetTree, TEXT("DoneKeybindsBtn"), TEXT("Done"));
	DoneBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleDoneKeybindsClicked);
	if (UHorizontalBoxSlot* ResetSlot = BindFooter->AddChildToHorizontalBox(ResetBtn))
	{
		ResetSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	BindFooter->AddChildToHorizontalBox(DoneBtn);
	AddPadded(KeybindsBox, BindFooter, 8.f);

	UBorder* ListenPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ListenPanel"));
	ListenPanel->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.12f, 0.98f));
	ListenPanel->SetPadding(FMargin(12.f));
	ListenPanel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* ListenCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ListenCol"));
	UTextBlock* ListenText = MakeLabel(WidgetTree, TEXT("ListenText"), TEXT("Press a key. Esc cancels. Alt + key for a combo."), 13);
	AddPadded(ListenCol, ListenText, 8.f);
	UHorizontalBox* ListenButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ListenButtons"));
	UButton* AcceptBtn = MakeTextButton(WidgetTree, TEXT("AcceptBind"), TEXT("Accept"));
	AcceptBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleAcceptBindClicked);
	UButton* CancelBtn = MakeTextButton(WidgetTree, TEXT("CancelListen"), TEXT("Cancel"));
	CancelBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleCancelListenClicked);
	if (UHorizontalBoxSlot* ASlot = ListenButtons->AddChildToHorizontalBox(AcceptBtn))
	{
		ASlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	ListenButtons->AddChildToHorizontalBox(CancelBtn);
	ListenButtons->SetVisibility(ESlateVisibility::Collapsed);
	AddPadded(ListenCol, ListenButtons, 0.f);
	ListenPanel->AddChild(ListenCol);
	AddPadded(KeybindsBox, ListenPanel, 8.f);

	UBorder* RebindPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RebindPanel"));
	RebindPanel->SetBrushColor(FLinearColor(0.16f, 0.1f, 0.06f, 0.98f));
	RebindPanel->SetPadding(FMargin(12.f));
	RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* RebindCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RebindCol"));
	UTextBlock* RebindText = MakeLabel(WidgetTree, TEXT("RebindText"), TEXT("Would you like to now re-bind the other key?"), 13);
	AddPadded(RebindCol, RebindText, 8.f);
	UHorizontalBox* RebindBtns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RebindBtns"));
	UButton* YesBtn = MakeTextButton(WidgetTree, TEXT("RebindYes"), TEXT("Yes"));
	YesBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleRebindYesClicked);
	UButton* NoBtn = MakeTextButton(WidgetTree, TEXT("RebindNo"), TEXT("No"));
	NoBtn->OnClicked.AddDynamic(this, &UCLMainMenuOverlay::HandleRebindNoClicked);
	if (UHorizontalBoxSlot* YSlot = RebindBtns->AddChildToHorizontalBox(YesBtn))
	{
		YSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	RebindBtns->AddChildToHorizontalBox(NoBtn);
	AddPadded(RebindCol, RebindBtns, 0.f);
	RebindPanel->AddChild(RebindCol);
	AddPadded(KeybindsBox, RebindPanel, 0.f);

	AddPadded(RootCol, KeybindsBox, 0.f);
	if (KeybindEditor)
	{
		KeybindEditor->BindWidgets(KeybindsBox, BindScroll, ListenPanel, ListenText, ListenButtons, RebindPanel, RebindText,
			BindLabels, PrimaryLabels, SecondaryLabels, GamepadLabels);
	}
}

void UCLMainMenuOverlay::ToggleOverlay()
{
	if (bVisible)
	{
		HideOverlay();
	}
	else
	{
		ShowOverlay();
	}
}

void UCLMainMenuOverlay::ShowOverlay()
{
	bVisible = true;
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
	if (!IsInViewport())
	{
		AddToViewport(100);
	}
	SetKeyboardFocus();
	ShowDirectorTab();
	if (KeybindEditor)
	{
		KeybindEditor->RefreshBindRows();
		KeybindEditor->CancelListen();
		KeybindEditor->HideRebindPanel();
	}
}

void UCLMainMenuOverlay::HideOverlay()
{
	CancelListen();
	bVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCLMainMenuOverlay::ShowDirectorTab()
{
	if (DirectorBox)
	{
		DirectorBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (KeybindsBox)
	{
		KeybindsBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCLMainMenuOverlay::ShowKeybindsTab()
{
	if (DirectorBox)
	{
		DirectorBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (KeybindsBox)
	{
		KeybindsBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (KeybindEditor)
	{
		KeybindEditor->RefreshBindRows();
	}
	SetKeyboardFocus();
}

bool UCLMainMenuOverlay::IsListening() const
{
	return KeybindEditor && KeybindEditor->IsListening();
}

void UCLMainMenuOverlay::HandleBindSlotClicked(ECLBindableAction Action, ECLBindColumn Column, bool bClear)
{
	if (KeybindEditor)
	{
		KeybindEditor->HandleBindSlotClicked(Action, Column, bClear);
	}
}

void UCLMainMenuOverlay::CancelListen()
{
	if (KeybindEditor)
	{
		KeybindEditor->CancelListen();
	}
}

FReply UCLMainMenuOverlay::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!IsListening())
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}
	return KeybindEditor ? KeybindEditor->OnKeyDown(InKeyEvent) : FReply::Handled();
}

FReply UCLMainMenuOverlay::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsListening())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	return KeybindEditor ? KeybindEditor->OnMouseButtonDown(InMouseEvent) : FReply::Handled();
}

FReply UCLMainMenuOverlay::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsListening())
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}
	return KeybindEditor ? KeybindEditor->OnMouseWheel(InMouseEvent) : FReply::Handled();
}

void UCLMainMenuOverlay::HandleDirectorTabClicked() { ShowDirectorTab(); }
void UCLMainMenuOverlay::HandleKeybindsTabClicked() { ShowKeybindsTab(); }
void UCLMainMenuOverlay::HandleComposeClicked() { JumpToActivity(ECLSceneId::Composer); }
void UCLMainMenuOverlay::HandlePvpClicked() { JumpToActivity(ECLSceneId::Pvp); }
void UCLMainMenuOverlay::HandleRaidClicked() { JumpToActivity(ECLSceneId::Raid); }
void UCLMainMenuOverlay::HandlePracticeClicked() { JumpToActivity(ECLSceneId::Practice); }

void UCLMainMenuOverlay::HandleReadyClicked()
{
	if (DirectorPanel)
	{
		DirectorPanel->ToggleLocalReady();
	}
}

void UCLMainMenuOverlay::HandleJoinRedClicked()
{
	if (DirectorPanel)
	{
		DirectorPanel->JoinTeam(ECLPvpTeam::Red);
	}
}

void UCLMainMenuOverlay::HandleJoinBlueClicked()
{
	if (DirectorPanel)
	{
		DirectorPanel->JoinTeam(ECLPvpTeam::Blue);
	}
}

void UCLMainMenuOverlay::HandleGoClicked()
{
	if (DirectorPanel)
	{
		DirectorPanel->RequestGo();
	}
}

void UCLMainMenuOverlay::HandleHostSocialOpenClicked()
{
	HostSocialLobby(ECLSocialPvpMode::Optional);
}

void UCLMainMenuOverlay::HandleHostSocialClosedClicked()
{
	if (DirectorPanel)
	{
		DirectorPanel->HostSocialClosed();
	}
}

void UCLMainMenuOverlay::HandleExitSocialClicked() { ExitToSocial(); }

void UCLMainMenuOverlay::HandleAcceptBindClicked()
{
	if (KeybindEditor)
	{
		KeybindEditor->HandleAcceptBindClicked();
	}
}

void UCLMainMenuOverlay::HandleCancelListenClicked()
{
	CancelListen();
}

void UCLMainMenuOverlay::HandleRebindYesClicked()
{
	if (KeybindEditor)
	{
		KeybindEditor->HandleRebindYesClicked();
	}
}

void UCLMainMenuOverlay::HandleRebindNoClicked()
{
	if (KeybindEditor)
	{
		KeybindEditor->HandleRebindNoClicked();
	}
}

void UCLMainMenuOverlay::HandleResetDefaultsClicked()
{
	if (KeybindEditor)
	{
		KeybindEditor->HandleResetDefaultsClicked();
	}
}

void UCLMainMenuOverlay::HandleDoneKeybindsClicked()
{
	if (KeybindEditor)
	{
		KeybindEditor->CancelListen();
		KeybindEditor->HideRebindPanel();
	}
	ShowDirectorTab();
}

void UCLMainMenuOverlay::JumpToActivity(ECLSceneId Scene, int32 RaidChamberIndex)
{
	if (DirectorPanel)
	{
		DirectorPanel->JumpToActivity(Scene, RaidChamberIndex);
	}
}

void UCLMainMenuOverlay::ExitToSocial()
{
	if (DirectorPanel)
	{
		DirectorPanel->ExitToSocial();
	}
}

void UCLMainMenuOverlay::UnsetDefaultProfile()
{
	if (DirectorPanel)
	{
		DirectorPanel->UnsetDefaultProfile();
	}
}

void UCLMainMenuOverlay::HostSocialLobby(ECLSocialPvpMode Mode)
{
	if (DirectorPanel)
	{
		DirectorPanel->HostSocialLobby(Mode);
	}
}

void UCLMainMenuOverlay::RefreshActivityLobbies(ECLSceneId Activity)
{
	if (DirectorPanel)
	{
		DirectorPanel->RefreshActivityLobbies(Activity);
	}
}

void UCLMainMenuOverlay::JoinListedLobby(int32 Index)
{
	if (DirectorPanel)
	{
		DirectorPanel->JoinListedLobby(Index);
	}
}

