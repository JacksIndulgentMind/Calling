#include "UI/DLComposerMenu.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLLobbyTypes.h"
#include "Game/DLParticipantSeat.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

	const TCHAR* TeamName(EDLPvpTeam Team)
	{
		switch (Team)
		{
		case EDLPvpTeam::Blue: return TEXT("Blue");
		case EDLPvpTeam::Red: return TEXT("Red");
		default: return TEXT("—");
		}
	}
}

TSharedRef<SWidget> UDLComposerMenu::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void UDLComposerMenu::BuildWidgetTree()
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
	HostButton->OnClicked.AddDynamic(this, &UDLComposerMenu::HandleHostClicked);
	AddPadded(Col, HostButton, 6.f);

	GuestButton = MakeTextButton(WidgetTree, TEXT("GuestBtn"), TEXT("Join as guest"), nullptr);
	GuestButton->OnClicked.AddDynamic(this, &UDLComposerMenu::HandleGuestClicked);
	AddPadded(Col, GuestButton, 6.f);

	ReadyButton = MakeTextButton(WidgetTree, TEXT("ReadyBtn"), TEXT("Ready"), &ReadyButtonLabel);
	ReadyButton->OnClicked.AddDynamic(this, &UDLComposerMenu::HandleReadyClicked);
	AddPadded(Col, ReadyButton, 6.f);

	RedButton = MakeTextButton(WidgetTree, TEXT("RedBtn"), TEXT("Join Red"), nullptr);
	RedButton->OnClicked.AddDynamic(this, &UDLComposerMenu::HandleJoinRedClicked);
	AddPadded(Col, RedButton, 6.f);

	BlueButton = MakeTextButton(WidgetTree, TEXT("BlueBtn"), TEXT("Join Blue"), nullptr);
	BlueButton->OnClicked.AddDynamic(this, &UDLComposerMenu::HandleJoinBlueClicked);
	AddPadded(Col, BlueButton, 6.f);

	StartButton = MakeTextButton(WidgetTree, TEXT("StartBtn"), TEXT("Start match"), nullptr);
	StartButton->OnClicked.AddDynamic(this, &UDLComposerMenu::HandleStartClicked);
	AddPadded(Col, StartButton, 0.f);
}

void UDLComposerMenu::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &UDLComposerMenu::Refresh, 0.25f, true);
	}
	Refresh();
}

void UDLComposerMenu::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::NativeDestruct();
}

void UDLComposerMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= 0.25f)
	{
		RefreshAccum = 0.f;
		Refresh();
	}
}

void UDLComposerMenu::Refresh()
{
	UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr;
	if (!Lobby)
	{
		return;
	}

	UDLParticipantSeat* Local = Lobby->FindLocalSeat();
	const bool bLocalHost = Local && Local->IsHost();
	if (RoleLabel)
	{
		RoleLabel->SetText(FText::FromString(bLocalHost
			? TEXT("You are the host. Ready, then Start when the lobby is full enough.")
			: TEXT("You are a guest. Ready up and wait for the host to Start.")));
	}

	const int32 Ready = Lobby->ReadyCount();
	const int32 Min = Lobby->GetInvoice() ? Lobby->GetInvoice()->MinPlayers : 2;
	const int32 Seats = Lobby->GetSeats().Num();
	FString Status;
	if (Lobby->IsMatchStartQueued() || Lobby->IsCountdownRunning())
	{
		Status = FString::Printf(TEXT("Starting in %.1fs"), Lobby->GetCountdownRemaining());
	}
	else if (Ready < Min)
	{
		Status = FString::Printf(TEXT("%d / %d ready  (%d in lobby, need %d)"), Ready, Min, Seats, Min);
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

	FString Rows = TEXT("Seats\n");
	for (const UDLParticipantSeat* Seat : Lobby->GetSeats())
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
	if (SeatListLabel)
	{
		SeatListLabel->SetText(FText::FromString(Rows));
	}

	const bool bLocked = Lobby->IsReadyLocked();
	if (ReadyButton)
	{
		ReadyButton->SetIsEnabled(!bLocked);
	}
	if (ReadyButtonLabel && Local)
	{
		ReadyButtonLabel->SetText(FText::FromString(Local->IsReady() ? TEXT("Unready") : TEXT("Ready")));
	}
	if (StartButton)
	{
		StartButton->SetIsEnabled(bLocalHost && Ready >= Min && !Lobby->IsMatchStartQueued());
		StartButton->SetVisibility(bLocalHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HostButton)
	{
		HostButton->SetIsEnabled(!bLocalHost && !bLocked);
	}
	if (GuestButton)
	{
		GuestButton->SetIsEnabled(bLocalHost && !bLocked);
	}
}

void UDLComposerMenu::HandleReadyClicked()
{
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		Lobby->ToggleLocalReady();
	}
	Refresh();
}

void UDLComposerMenu::HandleStartClicked()
{
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		Lobby->RequestLocalGo();
	}
	Refresh();
}

void UDLComposerMenu::HandleHostClicked()
{
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		Lobby->ClaimLocalHost();
	}
	Refresh();
}

void UDLComposerMenu::HandleGuestClicked()
{
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		Lobby->ClaimLocalGuest();
	}
	Refresh();
}

void UDLComposerMenu::HandleJoinRedClicked()
{
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		if (UDLParticipantSeat* Local = Lobby->FindLocalSeat())
		{
			FString Error;
			Lobby->SetTeam(Local->GetSeatId(), EDLPvpTeam::Red, Error);
		}
	}
	Refresh();
}

void UDLComposerMenu::HandleJoinBlueClicked()
{
	if (UDLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDLLobbySubsystem>() : nullptr)
	{
		if (UDLParticipantSeat* Local = Lobby->FindLocalSeat())
		{
			FString Error;
			Lobby->SetTeam(Local->GetSeatId(), EDLPvpTeam::Blue, Error);
		}
	}
	Refresh();
}
