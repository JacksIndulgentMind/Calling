#include "Player/DLPlayerController.h"
#include "Core/DLLog.h"
#include "Player/DLPlayerActionRouter.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLWeaponMotorComponent.h"
#include "Player/DLAbilityLoadoutComponent.h"
#include "UI/DLMainMenuOverlay.h"
#include "UI/DLCombatHudWidget.h"
#include "Game/DLGameModeBase.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLInputBindSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Core/DLTypes.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "ShowFlags.h"

ADLPlayerController::ADLPlayerController()
{
	MainMenuClass = UDLMainMenuOverlay::StaticClass();
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ADLPlayerController::BeginPlay()
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
	RestoreLitView();
}

void ADLPlayerController::RestoreLitView()
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

void ADLPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureCombatHud();
}

void ADLPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	EnsureCombatHud();
}

void ADLPlayerController::EnsureCombatHud()
{
	const bool bPlayerPawn = Cast<ADLPlayerCharacter>(GetPawn()) != nullptr;
	bool bBoot = false;
	if (UWorld* World = GetWorld())
	{
		if (const ADLGameModeBase* GM = Cast<ADLGameModeBase>(World->GetAuthGameMode()))
		{
			bBoot = GM->GetSceneId() == EDLSceneId::Boot;
		}
	}
	const bool bMenu = MainMenuInstance && MainMenuInstance->IsOverlayVisible();
	const bool bShow = bPlayerPawn && !bBoot && !bMenu;

	if (bShow && !CombatHud)
	{
		CombatHud = CreateWidget<UDLCombatHudWidget>(this, UDLCombatHudWidget::StaticClass());
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

void ADLPlayerController::EnsureMainMenu()
{
	if (!MainMenuInstance && MainMenuClass)
	{
		MainMenuInstance = CreateWidget<UDLMainMenuOverlay>(this, MainMenuClass);
	}
	if (MainMenuInstance && !MainMenuInstance->IsInViewport())
	{
		MainMenuInstance->AddToViewport(100);
		MainMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UDLMainMenuOverlay* ADLPlayerController::GetMainMenu()
{
	EnsureMainMenu();
	return MainMenuInstance;
}

void ADLPlayerController::ApplyMenuInputMode(bool bOpen)
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

void ADLPlayerController::SetMainMenuOpen(bool bOpen)
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

void ADLPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADLPlayerController::OnMove);
			EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADLPlayerController::OnMove);
		}
		if (LookAction)
		{
			EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADLPlayerController::OnLook);
		}
	}

	if (!MoveAction || !LookAction)
	{
		BindMoveLookKeys();
	}
}

void ADLPlayerController::BindMoveLookKeys()
{
	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindAxisKey(EKeys::MouseX, this, &ADLPlayerController::NativeLookYaw);
	InputComponent->BindAxisKey(EKeys::MouseY, this, &ADLPlayerController::NativeLookPitch);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ADLPlayerController::NativeForwardPressed);
	InputComponent->BindKey(EKeys::W, IE_Released, this, &ADLPlayerController::NativeForwardReleased);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ADLPlayerController::NativeBackPressed);
	InputComponent->BindKey(EKeys::S, IE_Released, this, &ADLPlayerController::NativeBackReleased);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ADLPlayerController::NativeLeftPressed);
	InputComponent->BindKey(EKeys::A, IE_Released, this, &ADLPlayerController::NativeLeftReleased);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ADLPlayerController::NativeRightPressed);
	InputComponent->BindKey(EKeys::D, IE_Released, this, &ADLPlayerController::NativeRightReleased);
}

void ADLPlayerController::PollMenuKeys()
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

void ADLPlayerController::RebuildNativeMove()
{
	CurrentMove.X = (bNativeRight ? 1.f : 0.f) + (bNativeLeft ? -1.f : 0.f);
	CurrentMove.Y = (bNativeForward ? 1.f : 0.f) + (bNativeBack ? -1.f : 0.f);
}

void ADLPlayerController::SampleGamepadAxes(float DeltaTime)
{
	RebuildNativeMove();
	const float LX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float LY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	CurrentMove.X = FMath::Clamp(CurrentMove.X + LX, -1.f, 1.f);
	CurrentMove.Y = FMath::Clamp(CurrentMove.Y + LY, -1.f, 1.f);

	float Sens = 1.5f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("/Script/DestinyLike.DLCameraFeelSettings"), TEXT("ControllerLookSensitivity"), Sens, GGameIni);
	}
	const float Scale = Sens * 90.f * DeltaTime;
	const float RX = GetInputAnalogKeyState(EKeys::Gamepad_RightX);
	const float RY = GetInputAnalogKeyState(EKeys::Gamepad_RightY);
	CurrentLook.X += RX * Scale;
	CurrentLook.Y -= RY * Scale;
}

void ADLPlayerController::NativeForwardPressed() { bNativeForward = true; RebuildNativeMove(); }
void ADLPlayerController::NativeForwardReleased() { bNativeForward = false; RebuildNativeMove(); }
void ADLPlayerController::NativeBackPressed() { bNativeBack = true; RebuildNativeMove(); }
void ADLPlayerController::NativeBackReleased() { bNativeBack = false; RebuildNativeMove(); }
void ADLPlayerController::NativeLeftPressed() { bNativeLeft = true; RebuildNativeMove(); }
void ADLPlayerController::NativeLeftReleased() { bNativeLeft = false; RebuildNativeMove(); }
void ADLPlayerController::NativeRightPressed() { bNativeRight = true; RebuildNativeMove(); }
void ADLPlayerController::NativeRightReleased() { bNativeRight = false; RebuildNativeMove(); }

void ADLPlayerController::NativeLookYaw(float Value)
{
	CurrentLook.X += Value;
}

void ADLPlayerController::NativeLookPitch(float Value)
{
	CurrentLook.Y -= Value;
}

void ADLPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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

void ADLPlayerController::PushInputToPawn()
{
	if (ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(GetPawn()))
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
			{
				if (!Lobby->IsGameplayUnlocked() || Lobby->IsRemotelyDriven(Char))
				{
					return;
				}
			}
		}
		Char->AccumulateInput(CurrentMove, CurrentLook, bSprint, bCrouch, bADS, bFire);
	}
}

void ADLPlayerController::OnMove(const FInputActionValue& Value)
{
	CurrentMove = Value.Get<FVector2D>();
}

void ADLPlayerController::OnLook(const FInputActionValue& Value)
{
	CurrentLook += Value.Get<FVector2D>();
}

UDLInputBindSubsystem* ADLPlayerController::GetBinds() const
{
	if (UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDLInputBindSubsystem>();
	}
	return nullptr;
}

bool ADLPlayerController::ChordHeld(const FDLKeyChord& Chord, bool bAltDown) const
{
	if (!Chord.IsSet() || Chord.bAlt != bAltDown)
	{
		return false;
	}
	if (DLInput::IsMouseWheelKey(Chord.Key))
	{
		return WasInputKeyJustPressed(EKeys::MouseScrollUp) || WasInputKeyJustPressed(EKeys::MouseScrollDown);
	}
	return IsInputKeyDown(Chord.Key);
}

bool ADLPlayerController::ChordPressed(const FDLKeyChord& Chord, bool bAltDown) const
{
	if (!Chord.IsSet() || Chord.bAlt != bAltDown)
	{
		return false;
	}
	if (DLInput::IsMouseWheelKey(Chord.Key))
	{
		return WasInputKeyJustPressed(EKeys::MouseScrollUp) || WasInputKeyJustPressed(EKeys::MouseScrollDown);
	}
	return WasInputKeyJustPressed(Chord.Key);
}

void ADLPlayerController::SampleRemappableBinds()
{
	UDLInputBindSubsystem* Table = GetBinds();
	if (!Table)
	{
		return;
	}

	const bool bAltDown = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
	TArray<EDLBindableAction> Pulses;

	bFire = false;
	bADS = false;
	bSprint = false;
	bCrouch = false;

	for (const EDLBindableAction Action : DLInput::AllActions())
	{
		const FDLActionBinds Pair = Table->GetBinds(Action);
		const bool bHeld = ChordHeld(Pair.Primary, bAltDown)
			|| ChordHeld(Pair.Secondary, bAltDown)
			|| ChordHeld(Pair.Gamepad, Pair.Gamepad.bAlt);
		const bool bPressed = ChordPressed(Pair.Primary, bAltDown)
			|| ChordPressed(Pair.Secondary, bAltDown)
			|| ChordPressed(Pair.Gamepad, Pair.Gamepad.bAlt);
		if (DLInput::IsHoldAction(Action))
		{
			switch (Action)
			{
			case EDLBindableAction::Fire: bFire = bHeld; break;
			case EDLBindableAction::ADS: bADS = bHeld; break;
			case EDLBindableAction::Sprint: bSprint = bHeld; break;
			case EDLBindableAction::Crouch: bCrouch = bHeld; break;
			default: break;
			}
		}
		else if (bPressed)
		{
			Pulses.Add(Action);
		}
	}

	TMap<FName, TArray<EDLBindableAction>> Groups;
	TSet<EDLBindableAction> Rejected;
	for (const EDLBindableAction Action : Pulses)
	{
		if (!DLInput::IsExclusionCandidate(Action))
		{
			continue;
		}
		const FName Group = DLInput::GetExclusionGroup(Action);
		if (Group.IsNone())
		{
			continue;
		}
		Groups.FindOrAdd(Group).Add(Action);
	}
	for (const TPair<FName, TArray<EDLBindableAction>>& Pair : Groups)
	{
		if (Pair.Value.Num() >= 2)
		{
			for (const EDLBindableAction Action : Pair.Value)
			{
				Rejected.Add(Action);
			}
			UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike: exclusion group %s rejected %d pulses (later: reject tell)."),
				*Pair.Key.ToString(), Pair.Value.Num());
		}
	}

	for (const EDLBindableAction Action : Pulses)
	{
		if (!Rejected.Contains(Action))
		{
			FirePulse(Action);
		}
	}
}

void ADLPlayerController::FirePulse(EDLBindableAction Action)
{
	DLPlayerActionRouter::DispatchPulse(Cast<ADLPlayerCharacter>(GetPawn()), Action);
}

void ADLPlayerController::ToggleMainMenu()
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
