#include "UI/CLComposerMenu.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLLobbyTypes.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSessionSubsystem.h"
#include "Game/CLLoopbackJoin.h"
#include "Game/CLGameStateBase.h"
#include "Player/CLPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName& Name, const FString& Text, int32 FontSize)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return Label;
	}

	UButton* MakeTextButton(UWidgetTree* Tree, const FName& Name, const FString& Text, TObjectPtr<UTextBlock>* OutLabel)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%sLabel"), *Name.ToString()));
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 14;
		Label->SetFont(Font);
		Button->AddChild(Label);
		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(Label->Slot))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Center);
			BtnSlot->SetVerticalAlignment(VAlign_Center);
			BtnSlot->SetPadding(FMargin(10.f, 6.f));
		}
		if (OutLabel)
		{
			*OutLabel = Label;
		}
		return Button;
	}

	void AddPadded(UVerticalBox* Box, UWidget* Child, float Bottom)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	const TCHAR* TeamName(ECLPvpTeam Team)
	{
		switch (Team)
		{
		case ECLPvpTeam::Blue: return TEXT("Blue");
		case ECLPvpTeam::Red: return TEXT("Red");
		default: return TEXT("—");
		}
	}
}

TSharedRef<SWidget> UCLComposerMenu::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UCLComposerMenu::BuildWidgetTree()
{
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.92f));
	Panel->SetPadding(FMargin(24.f));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetPosition(FVector2D(0.f, 0.f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Col"));
	if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(Panel->AddChild(Col)))
	{
		BorderSlot->SetHorizontalAlignment(HAlign_Fill);
		BorderSlot->SetPadding(FMargin(0.f));
	}

	TitleLabel = MakeLabel(WidgetTree, TEXT("Title"), TEXT("Compose PvP"), 22);
	AddPadded(Col, TitleLabel, 4.f);

	RoleLabel = MakeLabel(WidgetTree, TEXT("Role"), TEXT("You are the host."), 13);
	AddPadded(Col, RoleLabel, 8.f);

	StatusLabel = MakeLabel(WidgetTree, TEXT("Status"), TEXT("Ready up. Start when everyone is ready."), 13);
	AddPadded(Col, StatusLabel, 8.f);

	SeatListLabel = MakeLabel(WidgetTree, TEXT("Seats"), TEXT(""), 12);
	AddPadded(Col, SeatListLabel, 10.f);

	HostButton = MakeTextButton(WidgetTree, TEXT("HostBtn"), TEXT("Host this lobby"), nullptr);
	HostButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleHostClicked);
	AddPadded(Col, HostButton, 6.f);

	GuestButton = MakeTextButton(WidgetTree, TEXT("GuestBtn"), TEXT("Join as guest"), nullptr);
	GuestButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleGuestClicked);
	AddPadded(Col, GuestButton, 6.f);

	VirtualHostButton = MakeTextButton(WidgetTree, TEXT("VirtHostBtn"), TEXT("Virtual host"), nullptr);
	VirtualHostButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleVirtualHostClicked);
	AddPadded(Col, VirtualHostButton, 6.f);

	JoinSourceCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("JoinSrc"));
	JoinSourceCombo->AddOption(TEXT("Loopback (127.0.0.1)"));
	JoinSourceCombo->AddOption(TEXT("Beacon"));
	JoinSourceCombo->SetSelectedOption(TEXT("Loopback (127.0.0.1)"));
	AddPadded(Col, JoinSourceCombo, 6.f);

	VirtualJoinButton = MakeTextButton(WidgetTree, TEXT("VirtJoinBtn"), TEXT("Virtual join"), nullptr);
	VirtualJoinButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleVirtualJoinClicked);
	AddPadded(Col, VirtualJoinButton, 6.f);

	ReadyButton = MakeTextButton(WidgetTree, TEXT("ReadyBtn"), TEXT("Ready"), &ReadyButtonLabel);
	ReadyButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleReadyClicked);
	AddPadded(Col, ReadyButton, 6.f);

	RedButton = MakeTextButton(WidgetTree, TEXT("RedBtn"), TEXT("Join Red"), nullptr);
	RedButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleJoinRedClicked);
	AddPadded(Col, RedButton, 6.f);

	BlueButton = MakeTextButton(WidgetTree, TEXT("BlueBtn"), TEXT("Join Blue"), nullptr);
	BlueButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleJoinBlueClicked);
	AddPadded(Col, BlueButton, 6.f);

	StartButton = MakeTextButton(WidgetTree, TEXT("StartBtn"), TEXT("Start match"), nullptr);
	StartButton->OnClicked.AddDynamic(this, &UCLComposerMenu::HandleStartClicked);
	AddPadded(Col, StartButton, 0.f);
}

void UCLComposerMenu::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UCLComposerMenu::Refresh, 0.25f, true);
	}
	Refresh();
}

void UCLComposerMenu::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UCLComposerMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= 0.25f)
	{
		RefreshAccum = 0.f;
		Refresh();
	}
}

void UCLComposerMenu::Refresh()
{
	UWorld* World = GetWorld();
	const ENetMode Net = World ? World->GetNetMode() : NM_Standalone;
	const bool bNetClient = Net == NM_Client;
	const bool bListen = Net == NM_ListenServer;
	const bool bShowLoop = CLLoopbackJoin::ShowUi();
	const ESlateVisibility LoopVis = (bShowLoop && !bNetClient && !bListen) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	const ESlateVisibility HostLoopVis = (bShowLoop && !bNetClient && !bListen) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (VirtualHostButton)
	{
		VirtualHostButton->SetVisibility(HostLoopVis);
	}
	if (VirtualJoinButton)
	{
		VirtualJoinButton->SetVisibility(LoopVis);
	}
	if (JoinSourceCombo)
	{
		JoinSourceCombo->SetVisibility(LoopVis);
	}

	bool bLocalHost = false;
	bool bLocalReady = false;
	int32 Ready = 0;
	int32 Min = 2;
	int32 SeatCount = 0;
	bool bQueued = false;
	float CountdownLeft = 0.f;
	bool bLocked = false;
	FString Rows = TEXT("Seats\n");

	UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	if (bNetClient)
	{
		const ACLGameStateBase* GS = World ? World->GetGameState<ACLGameStateBase>() : nullptr;
		if (!GS)
		{
			return;
		}
		Ready = GS->GetLobbyReady();
		Min = GS->GetLobbyMinPlayers();
		bQueued = GS->IsLobbyStartQueued();
		SeatCount = GS->GetLobbySeats().Num();
		for (const FCLLobbySeatSnap& Seat : GS->GetLobbySeats())
		{
			Rows += FString::Printf(
				TEXT("• %s  %s  %s  %s\n"),
				*Seat.DisplayName,
				TeamName(Seat.Team),
				Seat.bReady ? TEXT("READY") : TEXT("—"),
				Seat.bHost ? TEXT("host") : TEXT("guest"));
			if (!Seat.bHost)
			{
				bLocalReady = Seat.bReady;
			}
		}
	}
	else if (Lobby)
	{
		UCLParticipantSeat* Local = Lobby->FindLocalSeat();
		bLocalHost = Local && Local->IsHost();
		bLocalReady = Local && Local->IsReady();
		Ready = Lobby->ReadyCount();
		Min = Lobby->GetInvoice() ? Lobby->GetInvoice()->MinPlayers : 2;
		SeatCount = Lobby->GetSeats().Num();
		bQueued = Lobby->IsMatchStartQueued() || Lobby->IsCountdownRunning();
		CountdownLeft = Lobby->GetCountdownRemaining();
		bLocked = Lobby->IsReadyLocked();
		for (const UCLParticipantSeat* Seat : Lobby->GetSeats())
		{
			if (!Seat)
			{
				continue;
			}
			Rows += FString::Printf(
				TEXT("• %s  %s  %s  %s\n"),
				*Seat->GetDisplayName(),
				TeamName(Seat->GetTeam()),
				Seat->IsReady() ? TEXT("READY") : TEXT("—"),
				Seat->IsHost() ? TEXT("host") : TEXT("guest"));
		}
	}
	else
	{
		return;
	}

	if (RoleLabel)
	{
		RoleLabel->SetText(FText::FromString(bLocalHost
			? TEXT("You are the host. Ready, then Start when the lobby is full enough.")
			: TEXT("You are a guest. Ready up and wait for the host to Start.")));
	}

	FString Status;
	if (bQueued)
	{
		Status = CountdownLeft > 0.f
			? FString::Printf(TEXT("Starting in %.1fs"), CountdownLeft)
			: TEXT("Starting…");
	}
	else if (Ready < Min)
	{
		Status = FString::Printf(TEXT("%d / %d ready  (%d in lobby, need %d)"), Ready, Min, SeatCount, Min);
	}
	else if (bLocalHost)
	{
		Status = FString::Printf(TEXT("%d / %d ready — you can Start."), Ready, Min);
	}
	else
	{
		Status = FString::Printf(TEXT("%d / %d ready — waiting on host Start."), Ready, Min);
	}
	if (StatusLabel)
	{
		StatusLabel->SetText(FText::FromString(Status));
	}
	if (SeatListLabel)
	{
		SeatListLabel->SetText(FText::FromString(Rows));
	}

	if (ReadyButton)
	{
		ReadyButton->SetIsEnabled(!bLocked);
	}
	if (ReadyButtonLabel)
	{
		ReadyButtonLabel->SetText(FText::FromString(bLocalReady ? TEXT("Unready") : TEXT("Ready")));
	}
	if (StartButton)
	{
		StartButton->SetIsEnabled(bLocalHost && Ready >= Min && !bQueued);
		StartButton->SetVisibility(bLocalHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HostButton)
	{
		HostButton->SetIsEnabled(!bNetClient && !bLocalHost && !bLocked);
		HostButton->SetVisibility(bNetClient ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (GuestButton)
	{
		GuestButton->SetIsEnabled(!bNetClient && bLocalHost && !bLocked);
		GuestButton->SetVisibility(bNetClient ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UCLComposerMenu::HandleReadyClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			if (ACLPlayerController* PC = Cast<ACLPlayerController>(GetOwningPlayer()))
			{
				PC->ServerComposerReadyToggle();
			}
			Refresh();
			return;
		}
	}
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->ToggleLocalReady();
	}
	Refresh();
}

void UCLComposerMenu::HandleStartClicked()
{
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->RequestLocalGo();
	}
	Refresh();
}

void UCLComposerMenu::HandleHostClicked()
{
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->ClaimLocalHost();
	}
	Refresh();
}

void UCLComposerMenu::HandleGuestClicked()
{
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		Lobby->ClaimLocalGuest();
	}
	Refresh();
}

void UCLComposerMenu::HandleJoinRedClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			if (ACLPlayerController* PC = Cast<ACLPlayerController>(GetOwningPlayer()))
			{
				PC->ServerComposerTeam(ECLPvpTeam::Red);
			}
			Refresh();
			return;
		}
	}
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		if (UCLParticipantSeat* Local = Lobby->FindLocalSeat())
		{
			FString Error;
			Lobby->SetTeam(Local->GetSeatId(), ECLPvpTeam::Red, Error);
		}
	}
	Refresh();
}

void UCLComposerMenu::HandleJoinBlueClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			if (ACLPlayerController* PC = Cast<ACLPlayerController>(GetOwningPlayer()))
			{
				PC->ServerComposerTeam(ECLPvpTeam::Blue);
			}
			Refresh();
			return;
		}
	}
	if (UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr)
	{
		if (UCLParticipantSeat* Local = Lobby->FindLocalSeat())
		{
			FString Error;
			Lobby->SetTeam(Local->GetSeatId(), ECLPvpTeam::Blue, Error);
		}
	}
	Refresh();
}

void UCLComposerMenu::HandleVirtualHostClicked()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->StartComposerLoopbackHost();
		}
	}
}

void UCLComposerMenu::HandleVirtualJoinClicked()
{
	FString Selected;
	if (JoinSourceCombo)
	{
		Selected = JoinSourceCombo->GetSelectedOption();
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->JoinLoopback(Selected);
		}
	}
}
