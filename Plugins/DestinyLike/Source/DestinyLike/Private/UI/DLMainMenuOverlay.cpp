#include "UI/DLMainMenuOverlay.h"
#include "Game/DLActivityLauncher.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLSessionSubsystem.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLLobbyTypes.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLErrorBoundary.h"
#include "Core/DLError.h"
#include "Game/DLGameModeBase.h"
#include "Game/DLInputBindSubsystem.h"
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

	UDLInputBindSubsystem* BindTable(const UUserWidget* Widget)
	{
		const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UDLInputBindSubsystem>() : nullptr;
	}
}

void UDLBindSlotButton::Configure(UDLMainMenuOverlay* InOwner, EDLBindableAction InAction, EDLBindColumn InColumn, bool bInClear)
{
	Owner = InOwner;
	Action = InAction;
	Column = InColumn;
	bClear = bInClear;
	OnClicked.AddDynamic(this, &UDLBindSlotButton::HandleClicked);
}

void UDLBindSlotButton::HandleClicked()
{
	if (UDLMainMenuOverlay* Menu = Owner.Get())
	{
		Menu->HandleBindSlotClicked(Action, Column, bClear);
	}
}

TSharedRef<SWidget> UDLMainMenuOverlay::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UDLMainMenuOverlay::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
	bVisible = false;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionSubsystem* Sessions = GI->GetSubsystem<UDLSessionSubsystem>())
		{
			Sessions->OnSessionEvent.AddDynamic(this, &UDLMainMenuOverlay::HandleSessionEvent);
		}
	}
}

void UDLMainMenuOverlay::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionSubsystem* Sessions = GI->GetSubsystem<UDLSessionSubsystem>())
		{
			Sessions->OnSessionEvent.RemoveDynamic(this, &UDLMainMenuOverlay::HandleSessionEvent);
		}
	}
	Super::NativeDestruct();
}

void UDLMainMenuOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bListening)
	{
		return;
	}
	APlayerController* PC = GetOwningPlayer();
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
			FDLKeyChord Chord;
			Chord.Key = Key;
			ProposeChord(Chord);
			return;
		}
	}
}

void UDLMainMenuOverlay::HandleSessionEvent(bool bSuccess, const FString& Message)
{
	if (bSuccess)
	{
		return;
	}
	UDLErrorBoundary::ReportStatic(this, FDLError::Make(EDLErrorKind::User, TEXT("session"), Message));
}

void UDLMainMenuOverlay::BuildWidgetTree()
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
	DirectorTab->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleDirectorTabClicked);
	UButton* KeybindsTab = MakeTextButton(WidgetTree, TEXT("KeybindsTab"), TEXT("Keybinds"));
	KeybindsTab->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleKeybindsTabClicked);
	if (UHorizontalBoxSlot* T0 = Tabs->AddChildToHorizontalBox(DirectorTab))
	{
		T0->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	Tabs->AddChildToHorizontalBox(KeybindsTab);
	AddPadded(RootCol, Tabs, 16.f);

	BuildDirectorPanel(RootCol);
	BuildKeybindEditor(RootCol);
}

void UDLMainMenuOverlay::BuildDirectorPanel(UVerticalBox* RootCol)
{
	DirectorBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DirectorBox"));
	AddPadded(DirectorBox, MakeLabel(WidgetTree, TEXT("DirectorHint"), TEXT("Compose PvP opens the composer menu. Host or Guest, then Ready. Host Start launches PvP. Remote seats Ready/Start on the hub. Launch PvP is a solo arena skip."), 13), 12.f);

	UButton* ComposeBtn = MakeTextButton(WidgetTree, TEXT("ComposeBtn"), TEXT("Compose PvP"));
	ComposeBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleComposeClicked);
	AddPadded(DirectorBox, ComposeBtn);

	UButton* PvpBtn = MakeTextButton(WidgetTree, TEXT("PvpBtn"), TEXT("Launch PvP (skip)"));
	PvpBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandlePvpClicked);
	AddPadded(DirectorBox, PvpBtn);

	UButton* RaidBtn = MakeTextButton(WidgetTree, TEXT("RaidBtn"), TEXT("Launch Raid"));
	RaidBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleRaidClicked);
	AddPadded(DirectorBox, RaidBtn);

	UButton* PracticeBtn = MakeTextButton(WidgetTree, TEXT("PracticeBtn"), TEXT("Launch Practice"));
	PracticeBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandlePracticeClicked);
	AddPadded(DirectorBox, PracticeBtn);

	UButton* ReadyBtn = MakeTextButton(WidgetTree, TEXT("ReadyBtn"), TEXT("Ready (toggle)"));
	ReadyBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleReadyClicked);
	AddPadded(DirectorBox, ReadyBtn);

	UButton* RedBtn = MakeTextButton(WidgetTree, TEXT("JoinRedBtn"), TEXT("Join Red"));
	RedBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleJoinRedClicked);
	AddPadded(DirectorBox, RedBtn);

	UButton* BlueBtn = MakeTextButton(WidgetTree, TEXT("JoinBlueBtn"), TEXT("Join Blue"));
	BlueBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleJoinBlueClicked);
	AddPadded(DirectorBox, BlueBtn);

	UButton* GoBtn = MakeTextButton(WidgetTree, TEXT("GoBtn"), TEXT("Host Go"));
	GoBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleGoClicked);
	AddPadded(DirectorBox, GoBtn);

	UButton* HostOpenBtn = MakeTextButton(WidgetTree, TEXT("HostOpenBtn"), TEXT("Host Social (open)"));
	HostOpenBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleHostSocialOpenClicked);
	AddPadded(DirectorBox, HostOpenBtn);

	UButton* HostClosedBtn = MakeTextButton(WidgetTree, TEXT("HostClosedBtn"), TEXT("Host Social (closed)"));
	HostClosedBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleHostSocialClosedClicked);
	AddPadded(DirectorBox, HostClosedBtn);

	UButton* SocialBtn = MakeTextButton(WidgetTree, TEXT("SocialBtn"), TEXT("Exit to Social"));
	SocialBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleExitSocialClicked);
	AddPadded(DirectorBox, SocialBtn, 0.f);
	AddPadded(RootCol, DirectorBox, 0.f);
}

void UDLMainMenuOverlay::BuildKeybindEditor(UVerticalBox* RootCol)
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

	BindScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BindScroll"));
	USizeBox* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScrollSize"));
	ScrollSize->SetHeightOverride(360.f);
	ScrollSize->AddChild(BindScroll);

	const TArray<EDLBindableAction>& Actions = DLInput::AllActions();
	BindLabels.SetNum(Actions.Num());
	PrimaryLabels.SetNum(Actions.Num());
	SecondaryLabels.SetNum(Actions.Num());
	GamepadLabels.SetNum(Actions.Num());

	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		const EDLBindableAction Action = Actions[i];
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("BindRow%d"), i));

		BindLabels[i] = MakeLabel(WidgetTree, *FString::Printf(TEXT("BindName%d"), i), DLInput::ActionDisplayName(Action), 13);
		if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(BindLabels[i]))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.2f;
			NameSlot->SetSize(Size);
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		UDLBindSlotButton* PrimBtn = WidgetTree->ConstructWidget<UDLBindSlotButton>(UDLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PrimBtn%d"), i));
		PrimaryLabels[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PrimLbl%d"), i));
		PrimaryLabels[i]->SetText(FText::FromString(TEXT("—")));
		StyleButtonLabel(PrimBtn, PrimaryLabels[i]);
		PrimBtn->Configure(this, Action, EDLBindColumn::Primary, false);
		if (UHorizontalBoxSlot* PSlot = Row->AddChildToHorizontalBox(PrimBtn))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.3f;
			PSlot->SetSize(Size);
			PSlot->SetPadding(FMargin(0.f, 2.f, 4.f, 2.f));
		}

		UDLBindSlotButton* PrimClear = WidgetTree->ConstructWidget<UDLBindSlotButton>(UDLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PrimClr%d"), i));
		UTextBlock* PrimClearLbl = MakeLabel(WidgetTree, *FString::Printf(TEXT("PrimClrLbl%d"), i), TEXT("Clear"), 12);
		StyleButtonLabel(PrimClear, PrimClearLbl);
		PrimClear->Configure(this, Action, EDLBindColumn::Primary, true);
		if (UHorizontalBoxSlot* PCSlot = Row->AddChildToHorizontalBox(PrimClear))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 0.5f;
			PCSlot->SetSize(Size);
			PCSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
		}

		UDLBindSlotButton* SecBtn = WidgetTree->ConstructWidget<UDLBindSlotButton>(UDLBindSlotButton::StaticClass(), *FString::Printf(TEXT("SecBtn%d"), i));
		SecondaryLabels[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("SecLbl%d"), i));
		SecondaryLabels[i]->SetText(FText::FromString(TEXT("—")));
		StyleButtonLabel(SecBtn, SecondaryLabels[i]);
		SecBtn->Configure(this, Action, EDLBindColumn::Secondary, false);
		if (UHorizontalBoxSlot* SSlot = Row->AddChildToHorizontalBox(SecBtn))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.3f;
			SSlot->SetSize(Size);
			SSlot->SetPadding(FMargin(0.f, 2.f, 4.f, 2.f));
		}

		UDLBindSlotButton* SecClear = WidgetTree->ConstructWidget<UDLBindSlotButton>(UDLBindSlotButton::StaticClass(), *FString::Printf(TEXT("SecClr%d"), i));
		UTextBlock* SecClearLbl = MakeLabel(WidgetTree, *FString::Printf(TEXT("SecClrLbl%d"), i), TEXT("Clear"), 12);
		StyleButtonLabel(SecClear, SecClearLbl);
		SecClear->Configure(this, Action, EDLBindColumn::Secondary, true);
		if (UHorizontalBoxSlot* SCSlot = Row->AddChildToHorizontalBox(SecClear))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 0.5f;
			SCSlot->SetSize(Size);
			SCSlot->SetPadding(FMargin(0.f, 2.f, 8.f, 2.f));
		}

		UDLBindSlotButton* PadBtn = WidgetTree->ConstructWidget<UDLBindSlotButton>(UDLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PadBtn%d"), i));
		GamepadLabels[i] = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("PadLbl%d"), i));
		GamepadLabels[i]->SetText(FText::FromString(TEXT("—")));
		StyleButtonLabel(PadBtn, GamepadLabels[i]);
		PadBtn->Configure(this, Action, EDLBindColumn::Gamepad, false);
		if (UHorizontalBoxSlot* GSlot = Row->AddChildToHorizontalBox(PadBtn))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.3f;
			GSlot->SetSize(Size);
			GSlot->SetPadding(FMargin(0.f, 2.f, 4.f, 2.f));
		}

		UDLBindSlotButton* PadClear = WidgetTree->ConstructWidget<UDLBindSlotButton>(UDLBindSlotButton::StaticClass(), *FString::Printf(TEXT("PadClr%d"), i));
		UTextBlock* PadClearLbl = MakeLabel(WidgetTree, *FString::Printf(TEXT("PadClrLbl%d"), i), TEXT("Clear"), 12);
		StyleButtonLabel(PadClear, PadClearLbl);
		PadClear->Configure(this, Action, EDLBindColumn::Gamepad, true);
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
	ResetBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleResetDefaultsClicked);
	UButton* DoneBtn = MakeTextButton(WidgetTree, TEXT("DoneKeybindsBtn"), TEXT("Done"));
	DoneBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleDoneKeybindsClicked);
	if (UHorizontalBoxSlot* ResetSlot = BindFooter->AddChildToHorizontalBox(ResetBtn))
	{
		ResetSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	BindFooter->AddChildToHorizontalBox(DoneBtn);
	AddPadded(KeybindsBox, BindFooter, 8.f);

	ListenPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ListenPanel"));
	ListenPanel->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.12f, 0.98f));
	ListenPanel->SetPadding(FMargin(12.f));
	ListenPanel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* ListenCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ListenCol"));
	ListenText = MakeLabel(WidgetTree, TEXT("ListenText"), TEXT("Press a key. Esc cancels. Alt + key for a combo."), 13);
	AddPadded(ListenCol, ListenText, 8.f);
	ListenButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ListenButtons"));
	UButton* AcceptBtn = MakeTextButton(WidgetTree, TEXT("AcceptBind"), TEXT("Accept"));
	AcceptBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleAcceptBindClicked);
	UButton* CancelBtn = MakeTextButton(WidgetTree, TEXT("CancelListen"), TEXT("Cancel"));
	CancelBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleCancelListenClicked);
	if (UHorizontalBoxSlot* ASlot = ListenButtons->AddChildToHorizontalBox(AcceptBtn))
	{
		ASlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	ListenButtons->AddChildToHorizontalBox(CancelBtn);
	ListenButtons->SetVisibility(ESlateVisibility::Collapsed);
	AddPadded(ListenCol, ListenButtons, 0.f);
	ListenPanel->AddChild(ListenCol);
	AddPadded(KeybindsBox, ListenPanel, 8.f);

	RebindPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RebindPanel"));
	RebindPanel->SetBrushColor(FLinearColor(0.16f, 0.1f, 0.06f, 0.98f));
	RebindPanel->SetPadding(FMargin(12.f));
	RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* RebindCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RebindCol"));
	RebindText = MakeLabel(WidgetTree, TEXT("RebindText"), TEXT("Would you like to now re-bind the other key?"), 13);
	AddPadded(RebindCol, RebindText, 8.f);
	UHorizontalBox* RebindBtns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RebindBtns"));
	UButton* YesBtn = MakeTextButton(WidgetTree, TEXT("RebindYes"), TEXT("Yes"));
	YesBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleRebindYesClicked);
	UButton* NoBtn = MakeTextButton(WidgetTree, TEXT("RebindNo"), TEXT("No"));
	NoBtn->OnClicked.AddDynamic(this, &UDLMainMenuOverlay::HandleRebindNoClicked);
	if (UHorizontalBoxSlot* YSlot = RebindBtns->AddChildToHorizontalBox(YesBtn))
	{
		YSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	RebindBtns->AddChildToHorizontalBox(NoBtn);
	AddPadded(RebindCol, RebindBtns, 0.f);
	RebindPanel->AddChild(RebindCol);
	AddPadded(KeybindsBox, RebindPanel, 0.f);

	AddPadded(RootCol, KeybindsBox, 0.f);
}

void UDLMainMenuOverlay::ToggleOverlay()
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

void UDLMainMenuOverlay::ShowOverlay()
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
	RefreshBindRows();
	CancelListen();
	if (RebindPanel)
	{
		RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDLMainMenuOverlay::HideOverlay()
{
	CancelListen();
	bVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDLMainMenuOverlay::ShowDirectorTab()
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

void UDLMainMenuOverlay::ShowKeybindsTab()
{
	if (DirectorBox)
	{
		DirectorBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (KeybindsBox)
	{
		KeybindsBox->SetVisibility(ESlateVisibility::Visible);
	}
	RefreshBindRows();
	SetKeyboardFocus();
}

void UDLMainMenuOverlay::RefreshBindRows()
{
	UDLInputBindSubsystem* Table = BindTable(this);
	if (!Table)
	{
		return;
	}
	const TArray<EDLBindableAction>& Actions = DLInput::AllActions();
	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		const FDLActionBinds Pair = Table->GetBinds(Actions[i]);
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

void UDLMainMenuOverlay::SetListenPrompt(const FString& Text, bool bWarning)
{
	if (ListenText)
	{
		ListenText->SetText(FText::FromString(Text));
		ListenText->SetColorAndOpacity(FSlateColor(bWarning
			? FLinearColor(1.f, 0.75f, 0.25f)
			: FLinearColor::White));
	}
}

void UDLMainMenuOverlay::HandleBindSlotClicked(EDLBindableAction Action, EDLBindColumn Column, bool bClear)
{
	UDLInputBindSubsystem* Table = BindTable(this);
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
	ProposedChord = FDLKeyChord();
	if (ListenPanel)
	{
		ListenPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (ListenButtons)
	{
		ListenButtons->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetListenPrompt(FString::Printf(TEXT("Press a key for %s (%s). Esc cancels. Alt + key for a combo. Triggers click past threshold."),
		*DLInput::ActionDisplayName(Action),
		*DLInput::ColumnDisplayName(Column)), false);
	SetKeyboardFocus();
}

void UDLMainMenuOverlay::CancelListen()
{
	bListening = false;
	bHasProposed = false;
	ProposedChord = FDLKeyChord();
	if (ListenPanel)
	{
		ListenPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ListenButtons)
	{
		ListenButtons->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDLMainMenuOverlay::ProposeChord(const FDLKeyChord& Chord)
{
	if (!Chord.IsSet())
	{
		return;
	}
	ProposedChord = Chord;
	bHasProposed = true;

	UDLInputBindSubsystem* Table = BindTable(this);
	FString Warn;
	bool bWarning = false;
	if (Table)
	{
		const TArray<FDLBindUse> Uses = Table->FindUses(Chord);
		for (const FDLBindUse& Use : Uses)
		{
			if (Use.Action == ListenAction && Use.Column == ListenColumn)
			{
				continue;
			}
			bWarning = true;
			const FDLBindUse SameCol = Table->FindSameColumnUse(Chord, ListenColumn, ListenAction);
			if (SameCol.bValid)
			{
				Warn = FString::Printf(TEXT("%s is used for %s (%s). Accept will unbind that column."),
					*Chord.ToDisplayString(),
					*DLInput::ActionDisplayName(SameCol.Action),
					*DLInput::ColumnDisplayName(SameCol.Column));
			}
			else
			{
				Warn = FString::Printf(TEXT("%s is also bound to %s (%s). Both will fire the same tick if they can."),
					*Chord.ToDisplayString(),
					*DLInput::ActionDisplayName(Use.Action),
					*DLInput::ColumnDisplayName(Use.Column));
			}
			break;
		}
	}

	if (Warn.IsEmpty())
	{
		SetListenPrompt(FString::Printf(TEXT("Bind %s to %s (%s)?"),
			*Chord.ToDisplayString(),
			*DLInput::ActionDisplayName(ListenAction),
			*DLInput::ColumnDisplayName(ListenColumn)), false);
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

bool UDLMainMenuOverlay::TryAcceptProposed()
{
	UDLInputBindSubsystem* Table = BindTable(this);
	if (!Table || !bHasProposed || !ProposedChord.IsSet())
	{
		return false;
	}

	FDLBindUse Stolen;
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
				*DLInput::ActionDisplayName(Stolen.Action),
				*DLInput::ColumnDisplayName(Stolen.Column))));
		}
		if (RebindPanel)
		{
			RebindPanel->SetVisibility(ESlateVisibility::Visible);
		}
		return true;
	}
	return true;
}

FReply UDLMainMenuOverlay::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bListening)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		CancelListen();
		return FReply::Handled();
	}
	if (DLInput::IsAltKey(Key) || DLInput::IsReservedMenuKey(Key))
	{
		return FReply::Handled();
	}

	FDLKeyChord Chord;
	Chord.Key = Key;
	Chord.bAlt = InKeyEvent.IsAltDown();
	ProposeChord(Chord);
	return FReply::Handled();
}

FReply UDLMainMenuOverlay::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bListening)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	FDLKeyChord Chord;
	Chord.Key = InMouseEvent.GetEffectingButton();
	Chord.bAlt = InMouseEvent.IsAltDown();
	if (DLInput::IsAltKey(Chord.Key))
	{
		return FReply::Handled();
	}
	ProposeChord(Chord);
	return FReply::Handled();
}

FReply UDLMainMenuOverlay::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bListening)
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	FDLKeyChord Chord;
	Chord.Key = InMouseEvent.GetWheelDelta() >= 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown;
	Chord.bAlt = InMouseEvent.IsAltDown();
	ProposeChord(Chord);
	return FReply::Handled();
}

void UDLMainMenuOverlay::HandleDirectorTabClicked() { ShowDirectorTab(); }
void UDLMainMenuOverlay::HandleKeybindsTabClicked() { ShowKeybindsTab(); }
void UDLMainMenuOverlay::HandleComposeClicked() { JumpToActivity(EDLSceneId::Composer); }
void UDLMainMenuOverlay::HandlePvpClicked() { JumpToActivity(EDLSceneId::Pvp); }
void UDLMainMenuOverlay::HandleRaidClicked() { JumpToActivity(EDLSceneId::Raid); }
void UDLMainMenuOverlay::HandlePracticeClicked() { JumpToActivity(EDLSceneId::Practice); }

void UDLMainMenuOverlay::HandleReadyClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->ToggleLocalReady();
		}
	}
}

void UDLMainMenuOverlay::HandleJoinRedClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (UDLParticipantSeat* Local = Lobby->FindLocalSeat())
			{
				FString Error;
				Lobby->SetTeam(Local->GetSeatId(), EDLPvpTeam::Red, Error);
			}
		}
	}
}

void UDLMainMenuOverlay::HandleJoinBlueClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (UDLParticipantSeat* Local = Lobby->FindLocalSeat())
			{
				FString Error;
				Lobby->SetTeam(Local->GetSeatId(), EDLPvpTeam::Blue, Error);
			}
		}
	}
}

void UDLMainMenuOverlay::HandleGoClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->RequestLocalGo();
		}
	}
	HideOverlay();
}

void UDLMainMenuOverlay::HandleHostSocialOpenClicked()
{
	HostSocialLobby(EDLSocialPvpMode::Optional);
}

void UDLMainMenuOverlay::HandleHostSocialClosedClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		UDLSessionSubsystem* Sessions = GI->GetSubsystem<UDLSessionSubsystem>();
		UDLSceneRouter* Router = GI->GetSubsystem<UDLSceneRouter>();
		if (Sessions && Router)
		{
			int32 MaxPlayers = 16;
			GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Social"), MaxPlayers, GGameIni);
			Sessions->HostSession(FDLLobbyInvoice::MakeSocial(EDLLobbyAccess::Closed, EDLSocialPvpMode::Optional, MaxPlayers),
				Router->GetMapNameForScene(EDLSceneId::Social));
		}
	}
}
void UDLMainMenuOverlay::HandleExitSocialClicked() { ExitToSocial(); }

void UDLMainMenuOverlay::HandleAcceptBindClicked()
{
	TryAcceptProposed();
}

void UDLMainMenuOverlay::HandleCancelListenClicked()
{
	CancelListen();
}

void UDLMainMenuOverlay::HandleRebindYesClicked()
{
	if (RebindPanel)
	{
		RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DisplacedUse.bValid)
	{
		HandleBindSlotClicked(DisplacedUse.Action, DisplacedUse.Column, false);
	}
	DisplacedUse = FDLBindUse();
}

void UDLMainMenuOverlay::HandleRebindNoClicked()
{
	if (RebindPanel)
	{
		RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	DisplacedUse = FDLBindUse();
}

void UDLMainMenuOverlay::HandleResetDefaultsClicked()
{
	if (UDLInputBindSubsystem* Table = BindTable(this))
	{
		Table->ResetDefaults();
		RefreshBindRows();
		CancelListen();
		if (RebindPanel)
		{
			RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UDLMainMenuOverlay::HandleDoneKeybindsClicked()
{
	CancelListen();
	if (RebindPanel)
	{
		RebindPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShowDirectorTab();
}

void UDLMainMenuOverlay::JumpToActivity(EDLSceneId Scene, int32 RaidChamberIndex)
{
	DLActivityLauncher::Travel(this, Scene, RaidChamberIndex);
	HideOverlay();
}

void UDLMainMenuOverlay::ExitToSocial()
{
	DLActivityLauncher::ExitToSocial(this);
	HideOverlay();
}

void UDLMainMenuOverlay::UnsetDefaultProfile()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLProfileSubsystem* Profiles = GI->GetSubsystem<UDLProfileSubsystem>())
		{
			const FDLLocalProfile Active = Profiles->GetActiveProfile();
			if (Active.ProfileId.IsValid())
			{
				Profiles->SetDefaultProfile(Active.ProfileId, false);
			}
		}
	}
}

void UDLMainMenuOverlay::HostSocialLobby(EDLSocialPvpMode Mode)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		UDLSessionSubsystem* Sessions = GI->GetSubsystem<UDLSessionSubsystem>();
		UDLSceneRouter* Router = GI->GetSubsystem<UDLSceneRouter>();
		if (Sessions && Router)
		{
			int32 MaxPlayers = 16;
			GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Social"), MaxPlayers, GGameIni);
			Sessions->HostSession(FDLLobbyInvoice::MakeSocial(EDLLobbyAccess::Open, Mode, MaxPlayers),
				Router->GetMapNameForScene(EDLSceneId::Social));
		}
	}
}

void UDLMainMenuOverlay::RefreshActivityLobbies(EDLSceneId Activity)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionSubsystem* Sessions = GI->GetSubsystem<UDLSessionSubsystem>())
		{
			Sessions->FindSessions(Activity);
		}
	}
}

void UDLMainMenuOverlay::JoinListedLobby(int32 Index)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionSubsystem* Sessions = GI->GetSubsystem<UDLSessionSubsystem>())
		{
			Sessions->JoinSessionByIndex(Index);
		}
	}
}
