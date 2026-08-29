#include "Player/CLPlayerController.h"
#include "Core/CLLog.h"
#include "Player/CLPlayerActionRouter.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLWeaponMotorComponent.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "UI/CLMainMenuOverlay.h"
#include "UI/CLCombatHudWidget.h"
#include "UI/CLComposerMenu.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLHubDriveTrace.h"
#include "Game/CLAgentCodec.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSessionSubsystem.h"
#include "Game/CLInputBindSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Core/CLTypes.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/UnrealNetwork.h"
#include "Game/CLInstanceIdentity.h"
#include "Engine/GameViewportClient.h"
#include "ShowFlags.h"

ACLPlayerController::ACLPlayerController()
{
	MainMenuClass = UCLMainMenuOverlay::StaticClass();
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ACLPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	EnsureCombatHud();
	EnsureMainMenu();
	EnsureComposerMenu();
	RestoreLitView();
	BindInstanceIdentity();
}

void ACLPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACLPlayerController, ReplicatedInstanceId);
}

void ACLPlayerController::BindInstanceIdentity()
{
	if (!IsLocalController())
	{
		return;
	}
	UGameInstance* GI = GetGameInstance();
	UCLInstanceIdentitySubsystem* Id = GI ? GI->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr;
	if (!Id)
	{
		return;
	}
	DeviceRequestorId = Id->GetDeviceRequestorId();
	if (HasAuthority())
	{
		ReplicatedInstanceId = Id->GetInstanceId();
	}
	else
	{
		ServerReportInstanceId(Id->GetInstanceId());
	}
}

void ACLPlayerController::ServerReportInstanceId_Implementation(FGuid Id)
{
	ReplicatedInstanceId = Id;
}

void ACLPlayerController::StampLocalDeviceRequestor()
{
	if (!DeviceRequestorId.IsValid() || !IsLocalController())
	{
		return;
	}
	UGameInstance* GI = GetGameInstance();
	UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	UCLParticipantSeat* Seat = Lobby ? Lobby->FindSeatForController(this) : nullptr;
	if (Seat)
	{
		Seat->SetRequestorId(DeviceRequestorId);
	}
}

void ACLPlayerController::RestoreLitView()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (UGameViewportClient* VC = World->GetGameViewport())
	{
		VC->ViewModeIndex = VMI_Lit;
		VC->EngineShowFlags.SetWireframe(false);
	}
}

void ACLPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureCombatHud();
}

void ACLPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	EnsureCombatHud();
}

void ACLPlayerController::EnsureCombatHud()
{
	const bool bPlayerPawn = Cast<ACLPlayerCharacter>(GetPawn()) != nullptr;
	bool bBoot = false;
	if (UWorld* World = GetWorld())
	{
		if (const ACLGameModeBase* GM = Cast<ACLGameModeBase>(World->GetAuthGameMode()))
		{
			bBoot = GM->GetSceneId() == ECLSceneId::Boot;
		}
	}
	const bool bMenu = MainMenuInstance && MainMenuInstance->IsOverlayVisible();
	const bool bShow = bPlayerPawn && !bBoot && !bMenu;

	if (bShow && !CombatHud)
	{
		CombatHud = CreateWidget<UCLCombatHudWidget>(this, UCLCombatHudWidget::StaticClass());
		if (CombatHud)
		{
			CombatHud->AddToViewport(5);
		}
	}
	if (CombatHud)
	{
		CombatHud->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShow)
		{
			CombatHud->PinToGameViewport();
			CombatHud->Refresh();
		}
	}
}

void ACLPlayerController::EnsureMainMenu()
{
	if (!MainMenuInstance && MainMenuClass)
	{
		MainMenuInstance = CreateWidget<UCLMainMenuOverlay>(this, MainMenuClass);
	}
	if (MainMenuInstance && !MainMenuInstance->IsInViewport())
	{
		MainMenuInstance->AddToViewport(100);
		MainMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UCLMainMenuOverlay* ACLPlayerController::GetMainMenu()
{
	EnsureMainMenu();
	return MainMenuInstance;
}

void ACLPlayerController::ApplyMenuInputMode(bool bOpen)
{
	bShowMouseCursor = bOpen;
	bEnableClickEvents = bOpen;
	bEnableMouseOverEvents = bOpen;
	if (bOpen && MainMenuInstance)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetWidgetToFocus(MainMenuInstance->TakeWidget());
		SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
	EnsureCombatHud();
}

void ACLPlayerController::SetMainMenuOpen(bool bOpen)
{
	EnsureMainMenu();
	if (!MainMenuInstance)
	{
		return;
	}
	if (bOpen)
	{
		MainMenuInstance->ShowOverlay();
	}
	else
	{
		MainMenuInstance->HideOverlay();
	}
	ApplyMenuInputMode(bOpen);
}

void ACLPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACLPlayerController::OnMove);
			EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ACLPlayerController::OnMove);
		}
		if (LookAction)
		{
			EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACLPlayerController::OnLook);
		}
	}

	if (!MoveAction || !LookAction)
	{
		BindMoveLookKeys();
	}
}

void ACLPlayerController::BindMoveLookKeys()
{
	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindAxisKey(EKeys::MouseX, this, &ACLPlayerController::NativeLookYaw);
	InputComponent->BindAxisKey(EKeys::MouseY, this, &ACLPlayerController::NativeLookPitch);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ACLPlayerController::NativeForwardPressed);
	InputComponent->BindKey(EKeys::W, IE_Released, this, &ACLPlayerController::NativeForwardReleased);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ACLPlayerController::NativeBackPressed);
	InputComponent->BindKey(EKeys::S, IE_Released, this, &ACLPlayerController::NativeBackReleased);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ACLPlayerController::NativeLeftPressed);
	InputComponent->BindKey(EKeys::A, IE_Released, this, &ACLPlayerController::NativeLeftReleased);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ACLPlayerController::NativeRightPressed);
	InputComponent->BindKey(EKeys::D, IE_Released, this, &ACLPlayerController::NativeRightReleased);
}

void ACLPlayerController::PollMenuKeys()
{
	const bool bHeld = IsInputKeyDown(EKeys::I)
		|| IsInputKeyDown(EKeys::Escape)
		|| IsInputKeyDown(EKeys::F1)
		|| IsInputKeyDown(EKeys::Gamepad_Special_Right);
	if (!bHeld)
	{
		bMenuKeyLatched = false;
		return;
	}
	if (bMenuKeyLatched)
	{
		return;
	}
	if (WasInputKeyJustPressed(EKeys::I)
		|| WasInputKeyJustPressed(EKeys::Escape)
		|| WasInputKeyJustPressed(EKeys::F1)
		|| WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
	{
		bMenuKeyLatched = true;
		if (WasInputKeyJustPressed(EKeys::F1))
		{
			RestoreLitView();
		}
		ToggleMainMenu();
	}
}

void ACLPlayerController::RebuildNativeMove()
{
	CurrentMove.X = (bNativeRight ? 1.f : 0.f) + (bNativeLeft ? -1.f : 0.f);
	CurrentMove.Y = (bNativeForward ? 1.f : 0.f) + (bNativeBack ? -1.f : 0.f);
}

void ACLPlayerController::SampleGamepadAxes(float DeltaTime)
{
	RebuildNativeMove();
	const float LX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float LY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	CurrentMove.X = FMath::Clamp(CurrentMove.X + LX, -1.f, 1.f);
	CurrentMove.Y = FMath::Clamp(CurrentMove.Y + LY, -1.f, 1.f);

	float Sens = 1.5f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/Calling.CLCameraFeelSettings"), TEXT("ControllerLookSensitivity"), Sens, GGameIni);
	}
	const float Scale = Sens * 90.f * DeltaTime;
	const float RX = GetInputAnalogKeyState(EKeys::Gamepad_RightX);
	const float RY = GetInputAnalogKeyState(EKeys::Gamepad_RightY);
	CurrentLook.X += RX * Scale;
	CurrentLook.Y -= RY * Scale;
}

void ACLPlayerController::NativeForwardPressed() { bNativeForward = true; RebuildNativeMove(); }
void ACLPlayerController::NativeForwardReleased() { bNativeForward = false; RebuildNativeMove(); }
void ACLPlayerController::NativeBackPressed() { bNativeBack = true; RebuildNativeMove(); }
void ACLPlayerController::NativeBackReleased() { bNativeBack = false; RebuildNativeMove(); }
void ACLPlayerController::NativeLeftPressed() { bNativeLeft = true; RebuildNativeMove(); }
void ACLPlayerController::NativeLeftReleased() { bNativeLeft = false; RebuildNativeMove(); }
void ACLPlayerController::NativeRightPressed() { bNativeRight = true; RebuildNativeMove(); }
void ACLPlayerController::NativeRightReleased() { bNativeRight = false; RebuildNativeMove(); }

void ACLPlayerController::NativeLookYaw(float Value)
{
	CurrentLook.X += Value;
}

void ACLPlayerController::NativeLookPitch(float Value)
{
	CurrentLook.Y -= Value;
}

void ACLPlayerController::EnsureComposerMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	ECLSceneId Scene = ECLSceneId::Social;
	if (UWorld* World = GetWorld())
	{
		if (const ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
		{
			Scene = GS->GetSceneId();
		}
		else if (const ACLGameModeBase* GM = Cast<ACLGameModeBase>(World->GetAuthGameMode()))
		{
			Scene = GM->GetSceneId();
		}
	}

	if (Scene != ECLSceneId::Composer)
	{
		if (ComposerMenuInstance)
		{
			ComposerMenuInstance->RemoveFromParent();
			ComposerMenuInstance = nullptr;
			if (!(MainMenuInstance && MainMenuInstance->IsOverlayVisible()))
			{
				ApplyMenuInputMode(false);
				ResetIgnoreInputFlags();
			}
		}
		return;
	}

	if (ComposerMenuInstance)
	{
		return;
	}

	ComposerMenuInstance = CreateWidget<UCLComposerMenu>(this, UCLComposerMenu::StaticClass());
	if (!ComposerMenuInstance)
	{
		return;
	}
	ComposerMenuInstance->AddToViewport(25);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetWidgetToFocus(ComposerMenuInstance->TakeWidget());
	SetInputMode(Mode);
}

void ACLPlayerController::ServerComposerReadyToggle_Implementation()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			if (UCLParticipantSeat* Seat = Lobby->FindSeatForController(this))
			{
				Lobby->SetReady(Seat->GetSeatId(), !Seat->IsReady());
			}
			else
			{
				Lobby->SetReadyForController(this, true);
			}
		}
	}
}

void ACLPlayerController::ServerComposerTeam_Implementation(ECLPvpTeam Team)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->SetTeamForController(this, Team);
		}
	}
}

void ACLPlayerController::ServerComposerReady_Implementation(bool bReady)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->SetReadyForController(this, bReady);
		}
	}
}

void ACLPlayerController::NoteHubReceive(int32 ListenPort, const FString& Recv, const FString& IntendedTarget)
{
	LastHubListenPort = ListenPort;
	LastHubRecv = Recv;
	LastHubIntendedTarget = IntendedTarget;
	const APawn* Body = GetPawn();
	const FCLHubDriveSnap& Snap = CLHubDriveTrace::Last();
	UE_LOG(LogCallingHub, Display,
		TEXT("PC HubRecv name=%s local=%d listenPort=%d recv=%s instance=%s agent=%s requestor=%s intended=%s pawnLocal=%d hasPawn=%d ignoreMove=%d ignoreLook=%d loc=(%.0f,%.0f,%.0f)"),
		*GetName(), IsLocalController() ? 1 : 0, ListenPort, *Recv,
		*Snap.InstanceId, *Snap.AgentId, *Snap.RequestorId, *IntendedTarget,
		Body && Body->IsLocallyControlled() ? 1 : 0, Body ? 1 : 0,
		IsMoveInputIgnored() ? 1 : 0, IsLookInputIgnored() ? 1 : 0,
		Body ? Body->GetActorLocation().X : 0.f, Body ? Body->GetActorLocation().Y : 0.f,
		Body ? Body->GetActorLocation().Z : 0.f);
}

void ACLPlayerController::ClientHubDispatch_Implementation(const FString& Json, int32 CorrelationId)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		CLHubDriveTrace::NotePlayerController(this, Root);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->HandleIncomingViaHub(Json, CorrelationId, this);
		}
	}
}

void ACLPlayerController::ServerHubDispatchResult_Implementation(int32 CorrelationId, const FString& Json)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
		{
			Lobby->CompleteHubVia(CorrelationId, Json);
		}
	}
}

void ACLPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsLocalController())
	{
		EnsureComposerMenu();
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
			{
				if (Sessions->IsLoopbackJoinPending())
				{
					UWorld* World = GetWorld();
					if (World && World->GetNetMode() == NM_Client)
					{
						ServerComposerReady(true);
						Sessions->ClearLoopbackJoinPending();
					}
				}
			}
		}
	}

	PollMenuKeys();

	const bool bMenu = MainMenuInstance && MainMenuInstance->IsOverlayVisible();
	if (bMenu)
	{
		CurrentLook = FVector2D::ZeroVector;
		EnsureCombatHud();
		return;
	}

	SampleGamepadAxes(DeltaTime);
	SampleRemappableBinds();
	PushInputToPawn();
	EnsureCombatHud();
	CurrentLook = FVector2D::ZeroVector;
}

void ACLPlayerController::PushInputToPawn()
{
	if (ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(GetPawn()))
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
			{
				if (!Lobby->IsGameplayUnlocked() || Lobby->IsRemotelyDriven(Char))
				{
					return;
				}
			}
		}
		Char->AccumulateInput(CurrentMove, CurrentLook, bSprint, bCrouch, bADS, bFire);
		if (!CurrentMove.IsNearlyZero())
		{
			StampLocalDeviceRequestor();
		}
	}
}

void ACLPlayerController::OnMove(const FInputActionValue& Value)
{
	CurrentMove = Value.Get<FVector2D>();
}

void ACLPlayerController::OnLook(const FInputActionValue& Value)
{
	CurrentLook += Value.Get<FVector2D>();
}

UCLInputBindSubsystem* ACLPlayerController::GetBinds() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UCLInputBindSubsystem>();
	}
	return nullptr;
}

bool ACLPlayerController::ChordHeld(const FCLKeyChord& Chord, bool bAltDown) const
{
	if (!Chord.IsSet() || Chord.bAlt != bAltDown)
	{
		return false;
	}
	if (CLInput::IsMouseWheelKey(Chord.Key))
	{
		return WasInputKeyJustPressed(EKeys::MouseScrollUp) || WasInputKeyJustPressed(EKeys::MouseScrollDown);
	}
	return IsInputKeyDown(Chord.Key);
}

bool ACLPlayerController::ChordPressed(const FCLKeyChord& Chord, bool bAltDown) const
{
	if (!Chord.IsSet() || Chord.bAlt != bAltDown)
	{
		return false;
	}
	if (CLInput::IsMouseWheelKey(Chord.Key))
	{
		return WasInputKeyJustPressed(EKeys::MouseScrollUp) || WasInputKeyJustPressed(EKeys::MouseScrollDown);
	}
	return WasInputKeyJustPressed(Chord.Key);
}

void ACLPlayerController::SampleRemappableBinds()
{
	UCLInputBindSubsystem* Table = GetBinds();
	if (!Table)
	{
		return;
	}

	const bool bAltDown = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
	TArray<ECLBindableAction> Pulses;

	bFire = false;
	bADS = false;
	bSprint = false;
	bCrouch = false;

	for (const ECLBindableAction Action : CLInput::AllActions())
	{
		const FCLActionBinds Pair = Table->GetBinds(Action);
		const bool bHeld = ChordHeld(Pair.Primary, bAltDown)
			|| ChordHeld(Pair.Secondary, bAltDown)
			|| ChordHeld(Pair.Gamepad, Pair.Gamepad.bAlt);
		const bool bPressed = ChordPressed(Pair.Primary, bAltDown)
			|| ChordPressed(Pair.Secondary, bAltDown)
			|| ChordPressed(Pair.Gamepad, Pair.Gamepad.bAlt);
		if (CLInput::IsHoldAction(Action))
		{
			switch (Action)
			{
			case ECLBindableAction::Fire: bFire = bHeld; break;
			case ECLBindableAction::ADS: bADS = bHeld; break;
			case ECLBindableAction::Sprint: bSprint = bHeld; break;
			case ECLBindableAction::Crouch: bCrouch = bHeld; break;
			default: break;
			}
		}
		else if (bPressed)
		{
			Pulses.Add(Action);
		}
	}

	TMap<FName, TArray<ECLBindableAction>> Groups;
	TSet<ECLBindableAction> Rejected;
	for (const ECLBindableAction Action : Pulses)
	{
		if (!CLInput::IsExclusionCandidate(Action))
		{
			continue;
		}
		const FName Group = CLInput::GetExclusionGroup(Action);
		if (Group.IsNone())
		{
			continue;
		}
		Groups.FindOrAdd(Group).Add(Action);
	}
	for (const TPair<FName, TArray<ECLBindableAction>>& Pair : Groups)
	{
		if (Pair.Value.Num() >= 2)
		{
			for (const ECLBindableAction Action : Pair.Value)
			{
				Rejected.Add(Action);
			}
			UE_LOG(LogCalling, Display, TEXT("Calling: exclusion group %s rejected %d pulses (later: reject tell)."),
				*Pair.Key.ToString(), Pair.Value.Num());
		}
	}

	for (const ECLBindableAction Action : Pulses)
	{
		if (!Rejected.Contains(Action))
		{
			FirePulse(Action);
		}
	}
}

void ACLPlayerController::FirePulse(ECLBindableAction Action)
{
	CLPlayerActionRouter::DispatchPulse(Cast<ACLPlayerCharacter>(GetPawn()), Action);
}

void ACLPlayerController::ToggleMainMenu()
{
	if (MainMenuInstance && MainMenuInstance->IsOverlayVisible() && MainMenuInstance->IsListening())
	{
		MainMenuInstance->CancelListen();
		return;
	}

	EnsureMainMenu();
	if (MainMenuInstance)
	{
		MainMenuInstance->ToggleOverlay();
		ApplyMenuInputMode(MainMenuInstance->IsOverlayVisible());
	}
}
