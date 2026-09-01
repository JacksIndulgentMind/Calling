#include "Game/CLLobbySubsystem.h"
#include "Game/CLLobbyTypes.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSeatMotor.h"
#include "AI/CLBotBookManager.h"
#include "Game/CLHubIngress.h"
#include "Game/CLHubCommandRegistry.h"
#include "Game/CLHubDriveTrace.h"
#include "Game/CLInstanceIdentity.h"
#include "Game/CLAgentCodec.h"
#include "Game/CLLoopbackJoin.h"
#include "Game/CLInvoiceService.h"
#include "Game/CLSeatRegistry.h"
#include "Game/CLGateCountdown.h"
#include "Game/CLTravelCoordinator.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLGameInstance.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLSessionHub.h"
#include "Game/CLSessionSubsystem.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLPlayerController.h"
#include "Player/CLCombatPawn.h"
#include "Player/CLPossessionComponent.h"
#include "Player/CLHeadlessAgent.h"
#include "AI/CLSeatController.h"
#include "Core/CLTickClock.h"
#include "Core/CLLog.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/PlayerStartPIE.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/CLTunes.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString GuidStr(const FGuid& Id)
	{
		return Id.IsValid() ? Id.ToString(EGuidFormats::DigitsWithHyphens) : FString();
	}

	FGuid ParseGuid(const FString& Text)
	{
		FGuid Id;
		FGuid::Parse(Text, Id);
		return Id;
	}

	bool JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool bDefault = false)
	{
		return Obj.IsValid() && Obj->HasTypedField<EJson::Boolean>(Key) ? Obj->GetBoolField(Key) : bDefault;
	}

	float JsonNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float Default = 0.f)
	{
		return Obj.IsValid() && Obj->HasField(Key) ? static_cast<float>(Obj->GetNumberField(Key)) : Default;
	}

	FString JsonStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default = FString())
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(Key, Value) ? Value : Default;
	}
}

void UCLLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UCLTickSubsystem::StaticClass());
	Super::Initialize(Collection);
	Invoices = NewObject<UCLInvoiceService>(this);
	SeatReg = NewObject<UCLSeatRegistry>(this);
	GateClock = NewObject<UCLGateCountdown>(this);
	Travel = NewObject<UCLTravelCoordinator>(this);
	GateClock->LoadLaunchSecondsFromConfig();
	BindClock();
}

void UCLLobbySubsystem::Deinitialize()
{
	UnbindClock();
	ClearScene();
	Super::Deinitialize();
}

void UCLLobbySubsystem::BindClock()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLTickSubsystem* Tick = GI->GetSubsystem<UCLTickSubsystem>())
		{
			NetTickHandle = Tick->OnNetTick().AddUObject(this, &UCLLobbySubsystem::TickNet);
		}
	}
}

void UCLLobbySubsystem::UnbindClock()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLTickSubsystem* Tick = GI->GetSubsystem<UCLTickSubsystem>())
		{
			Tick->OnNetTick().Remove(NetTickHandle);
		}
	}
	NetTickHandle = FDelegateHandle();
}

void UCLLobbySubsystem::SetPendingInvoice(const FCLLobbyInvoice& InInvoice)
{
	Invoices->SetPending(InInvoice);
}

void UCLLobbySubsystem::ClearPendingInvoice()
{
	Invoices->ClearPending();
}

const FCLLobbyInvoice* UCLLobbySubsystem::GetPendingInvoice() const
{
	return Invoices->GetPending();
}

const FCLLobbyInvoice* UCLLobbySubsystem::GetInvoice() const
{
	return Invoices->GetLive();
}

const FCLLobbyGate* UCLLobbySubsystem::GetGate() const
{
	return GateClock->GetGate();
}

FName UCLLobbySubsystem::GetLootRealmId() const
{
	return Invoices->GetLootRealmId();
}

bool UCLLobbySubsystem::IsGameplayUnlocked() const
{
	return GateClock->IsUnlocked();
}

bool UCLLobbySubsystem::HasInvoice() const
{
	return Invoices->GetLive() != nullptr;
}

bool UCLLobbySubsystem::HasGate() const
{
	return GateClock->HasGate();
}

bool UCLLobbySubsystem::IsMatchStartQueued() const
{
	return GateClock->IsMatchStartQueued();
}

bool UCLLobbySubsystem::IsCountdownRunning() const
{
	return GateClock->IsCountdownRunning();
}

float UCLLobbySubsystem::GetCountdownRemaining() const
{
	return GateClock->GetCountdownRemaining();
}

void UCLLobbySubsystem::ConsumePendingOrDefault(ECLSceneId Scene)
{
	Invoices->ConsumePendingOrDefault(Scene);
}

void UCLLobbySubsystem::ClearScene()
{
	SeatReg->Reset();
	Invoices->ClearLive();
	Invoices->ClearPending();
	GateClock->ResetOpen();
	LastJoinedSeatId.Invalidate();
}

void UCLLobbySubsystem::BeginOpenScene()
{
	SeatReg->Reset();
	GateClock->ResetOpen();
	Invoices->AdoptPending();
	EnsureLocalHumanSeat();
}

void UCLLobbySubsystem::BeginGatedScene(ECLSceneId Scene)
{
	SeatReg->Reset();
	GateClock->ResetLocked();
	ConsumePendingOrDefault(Scene);
	GateClock->InstallFromConfig();
	if (UCLParticipantSeat* Host = EnsureLocalHumanSeat())
	{
		Host->SetReady(true);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionHub* Hub = GI->GetSubsystem<UCLSessionHub>())
		{
			Hub->StartHost();
		}
	}
	StartCountdownIfReady();
}

void UCLLobbySubsystem::BeginComposerScene()
{
	SeatReg->Reset();
	GateClock->ResetOpen();
	if (Invoices->GetPending())
	{
		Invoices->AdoptPending();
	}
	else
	{
		ConsumePendingOrDefault(ECLSceneId::Composer);
	}
	if (UCLParticipantSeat* Host = EnsureLocalHumanSeat())
	{
		Host->SetReady(false);
		if (Host->GetTeam() == ECLPvpTeam::Unassigned)
		{
			Host->SetTeam(ECLPvpTeam::Red);
		}
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionHub* Hub = GI->GetSubsystem<UCLSessionHub>())
		{
			Hub->StartHost();
		}
	}
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
}

void UCLLobbySubsystem::BeginPvpOrRestore()
{
	if (Invoices->GetPending())
	{
		Invoices->AdoptPending();
	}
	const FCLLobbyInvoice* Live = Invoices->GetLive();
	const bool bFromComposer = SeatReg->Num() > 0 && Live && Live->Roster.Num() > 0;
	if (bFromComposer)
	{
		Invoices->SetLiveActivity(ECLSceneId::Pvp);
		GateClock->ResetOpen();
		RestoreBodiesAfterTravel();
		NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
		return;
	}
	BeginGatedScene(ECLSceneId::Pvp);
}

UCLParticipantSeat* UCLLobbySubsystem::EnsureLocalHumanSeat()
{
	FString Name = TEXT("Host");
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			const FString ProfileName = Profiles->GetActiveProfile().DisplayName;
			if (!ProfileName.IsEmpty())
			{
				Name = ProfileName;
			}
		}
	}
	return SeatReg->EnsureLocalHuman(Name, GateClock->GetGate());
}

UCLParticipantSeat* UCLLobbySubsystem::EnsureNetHumanSeat(APlayerController* PC)
{
	FString Name = TEXT("Player");
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			const FString ProfileName = Profiles->GetActiveProfile().DisplayName;
			if (!ProfileName.IsEmpty())
			{
				Name = ProfileName;
			}
		}
	}
	if (PC && !PC->IsLocalController())
	{
		Name = TEXT("Guest");
	}
	UCLParticipantSeat* Seat = SeatReg->EnsureNetHuman(PC, Name, GateClock->GetGate());
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	return Seat;
}

void UCLLobbySubsystem::RemoveSeatForController(AController* Controller)
{
	SeatReg->RemoveForController(Controller);
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
}

void UCLLobbySubsystem::PushLobbyToGameState()
{
	UWorld* World = GetWorld();
	if (!World || !World->GetAuthGameMode())
	{
		return;
	}
	ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>();
	if (!GS)
	{
		return;
	}
	TArray<FCLLobbySeatSnap> Snaps;
	for (const UCLParticipantSeat* Seat : SeatReg->GetAll())
	{
		if (!Seat)
		{
			continue;
		}
		FCLLobbySeatSnap Row;
		Row.SeatId = Seat->GetSeatId();
		Row.DisplayName = Seat->GetDisplayName();
		Row.Team = Seat->GetTeam();
		Row.bReady = Seat->IsReady();
		Row.bHost = Seat->IsHost();
		Snaps.Add(Row);
	}
	const int32 Min = GetInvoice() ? GetInvoice()->MinPlayers : 2;
	GS->SetLobbySnapshot(Snaps, ReadyCount(), Min, IsMatchStartQueued() || IsCountdownRunning());
}

bool UCLLobbySubsystem::SetReadyForController(APlayerController* PC, bool bReady)
{
	UCLParticipantSeat* Seat = EnsureNetHumanSeat(PC);
	if (!Seat)
	{
		return false;
	}
	return SetReady(Seat->GetSeatId(), bReady);
}

bool UCLLobbySubsystem::SetTeamForController(APlayerController* PC, ECLPvpTeam Team)
{
	UCLParticipantSeat* Seat = EnsureNetHumanSeat(PC);
	if (!Seat)
	{
		return false;
	}
	FString Error;
	return SetTeam(Seat->GetSeatId(), Team, Error);
}

ACLPlayerCharacter* UCLLobbySubsystem::FindHumanPawn() const
{
	return SeatReg->FindHumanPawn();
}

UCLParticipantSeat* UCLLobbySubsystem::JoinRemoteAgent(const FString& DisplayName, bool bHeadless, FString& OutError, const FString& Kind)
{
	if (const FCLLobbyInvoice* Live = GetInvoice())
	{
		if (SeatReg->Num() >= Live->MaxPlayers)
		{
			OutError = TEXT("lobby_full");
			return nullptr;
		}
	}

	const FString Name = DisplayName.IsEmpty() ? TEXT("agent") : DisplayName;
	const FString UseKind = Kind.IsEmpty() ? TEXT("remoteAgent") : Kind;
	UClass* MotorClass = UCLSeatRegistry::SeatMotorClassFromKind(UseKind);
	if (!MotorClass || !MotorClass->IsChildOf(UCLRemoteAgentSeatMotor::StaticClass()))
	{
		OutError = TEXT("not_remote_kind");
		return nullptr;
	}
	UCLParticipantSeat* Seat = SeatReg->MakeSeat(Name, MotorClass, FGuid(), GateClock->GetGate());
	UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("no_world");
		SeatReg->MutableSeats().Remove(Seat);
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (bHeadless)
	{
		if (World->GetNetMode() == NM_Client)
		{
			UCLPossessionComponent* Poss = NewObject<UCLPossessionComponent>(Seat);
			Poss->GoHeadless();
			Seat->SetPossession(Poss);
			Seat->SetHeadlessJoin(true);
		}
		else
		{
			ACLHeadlessAgent* Anchor = World->SpawnActor<ACLHeadlessAgent>(ACLHeadlessAgent::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (!Anchor)
			{
				OutError = TEXT("spawn_failed");
				SeatReg->MutableSeats().Remove(Seat);
				return nullptr;
			}
			Seat->SetAnchor(Anchor);
			Seat->SetPossession(Anchor->GetPossession());
			Seat->SetHeadlessJoin(true);
			Anchor->GetPossession()->GoHeadless();
		}
	}
	else
	{
		APawn* Pawn = SeatReg->SpawnAgentPawn(Seat->GetTeam());
		ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Pawn);
		if (!Char || !Char->GetPossession())
		{
			OutError = TEXT("spawn_failed");
			SeatReg->MutableSeats().Remove(Seat);
			return nullptr;
		}
		Char->GetPossession()->PossessOwn(Char);
		Seat->SetPossession(Char->GetPossession());
		Seat->SetAnchor(Char);
		Seat->SetHeadlessJoin(false);
		Seat->SetDriveSeatId(Seat->GetSeatId());
	}

	LastJoinedSeatId = Seat->GetSeatId();
	if (UCLInstanceIdentitySubsystem* Id = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr)
	{
		Id->BindSeat(Seat);
	}
	OutError.Reset();
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	UE_LOG(LogCalling, Display, TEXT("Calling: agent seat %s kind=%s headless=%d"),
		*GuidStr(Seat->GetSeatId()), *UseKind, bHeadless ? 1 : 0);
	return Seat;
}

UCLParticipantSeat* UCLLobbySubsystem::FindSeat(const FGuid& SeatId) const
{
	return SeatReg->Find(SeatId);
}

UCLParticipantSeat* UCLLobbySubsystem::FindSeatByName(const FString& DisplayName) const
{
	return SeatReg->FindByName(DisplayName);
}

UCLParticipantSeat* UCLLobbySubsystem::FindHostSeat() const
{
	return SeatReg->FindHost();
}

UCLParticipantSeat* UCLLobbySubsystem::FindLocalSeat() const
{
	return SeatReg->FindLocal();
}

UCLParticipantSeat* UCLLobbySubsystem::FindSeatForController(const AController* Controller) const
{
	return SeatReg->FindForController(Controller);
}

APawn* UCLLobbySubsystem::GetDrivenPawn(const FGuid& SeatId) const
{
	return SeatReg->GetDrivenPawn(SeatId);
}

bool UCLLobbySubsystem::IsRemotelyDriven(const APawn* Pawn) const
{
	return SeatReg->IsRemotelyDriven(Pawn);
}

int32 UCLLobbySubsystem::ReadyCount() const
{
	return SeatReg->ReadyCount();
}

TArray<UCLParticipantSeat*> UCLLobbySubsystem::GetSeats() const
{
	return SeatReg->GetAll();
}

bool UCLLobbySubsystem::IsReadyLocked() const
{
	return GateClock->IsReadyLocked();
}

bool UCLLobbySubsystem::SetReady(const FGuid& SeatId, bool bReady)
{
	if (IsReadyLocked())
	{
		return false;
	}
	UCLParticipantSeat* Seat = FindSeat(SeatId);
	if (!Seat)
	{
		if (UCLParticipantSeat* Host = EnsureLocalHumanSeat())
		{
			Seat = Host;
		}
	}
	if (!Seat)
	{
		return false;
	}
	Seat->SetReady(bReady);
	if (!bReady && !GateClock->IsMatchStartQueued())
	{
		GateClock->CancelCountdownIfUnready();
	}
	if (GateClock->HasGate() && !GateClock->IsUnlocked())
	{
		StartCountdownIfReady();
	}
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	return true;
}

bool UCLLobbySubsystem::ToggleLocalReady()
{
	UCLParticipantSeat* Local = FindLocalSeat();
	if (!Local)
	{
		Local = EnsureLocalHumanSeat();
	}
	if (!Local)
	{
		return false;
	}
	return SetReady(Local->GetSeatId(), !Local->IsReady());
}

bool UCLLobbySubsystem::ClaimLocalHost()
{
	UCLParticipantSeat* Local = EnsureLocalHumanSeat();
	if (!Local)
	{
		return false;
	}
	for (UCLParticipantSeat* Seat : SeatReg->GetAll())
	{
		if (Seat && Seat != Local)
		{
			Seat->SetHost(false);
		}
	}
	Local->SetHost(true);
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	return true;
}

bool UCLLobbySubsystem::ClaimLocalGuest()
{
	UCLParticipantSeat* Local = EnsureLocalHumanSeat();
	if (!Local)
	{
		return false;
	}
	Local->SetHost(false);
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	return true;
}

bool UCLLobbySubsystem::SetTeam(const FGuid& SeatId, ECLPvpTeam Team, FString& OutError)
{
	UCLParticipantSeat* Seat = FindSeat(SeatId);
	if (!Seat)
	{
		OutError = TEXT("no_seat");
		return false;
	}
	Seat->SetTeam(Team);
	OutError.Reset();
	NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	return true;
}

void UCLLobbySubsystem::StartCountdownIfReady()
{
	GateClock->StartCountdownIfReady(GetInvoice(), ReadyCount());
}

bool UCLLobbySubsystem::RequestLocalGo()
{
	const UCLParticipantSeat* Local = FindLocalSeat();
	if (!Local || !Local->IsHost())
	{
		return false;
	}
	return RequestGo();
}

bool UCLLobbySubsystem::RequestGo()
{
	UCLParticipantSeat* Host = FindHostSeat();
	if (!Host)
	{
		return false;
	}
	const bool bOk = GateClock->RequestGo(GetInvoice(), ReadyCount(), true, [this]() { FinishGo(); });
	if (bOk)
	{
		NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
	}
	return bOk;
}

void UCLLobbySubsystem::FinishGo()
{
	GateClock->FinishGo([this]()
	{
		NotifyHubSnapshots(ECLHubSnapshotReason::LobbyDirty);
		if (UWorld* World = GetWorld())
		{
			if (ACLGameModeBase* GM = World->GetAuthGameMode<ACLGameModeBase>())
			{
				GM->HandleLobbyGo();
			}
		}
	});
}

bool UCLLobbySubsystem::MindControl(const FGuid& AgentSeatId, const FGuid& TargetSeatId, FString& OutError)
{
	UCLParticipantSeat* Agent = FindSeat(AgentSeatId);
	UCLParticipantSeat* Target = FindSeat(TargetSeatId);
	if (!Agent || !Agent->GetSeatMotor() || !Agent->GetSeatMotor()->IsA<UCLRemoteAgentSeatMotor>())
	{
		OutError = TEXT("agent_only");
		return false;
	}
	APawn* TargetPawn = Target ? Target->GetDrivenPawn() : FindHumanPawn();
	if (!TargetPawn || !Agent->GetPossession())
	{
		OutError = TEXT("no_target");
		return false;
	}
	if (!TargetPawn->IsLocallyControlled())
	{
		OutError = TEXT("remote_pawn");
		return false;
	}
	Agent->GetPossession()->MindControl(TargetPawn);
	Agent->SetDriveSeatId(Target ? Target->GetSeatId() : FGuid());
	OutError.Reset();
	return true;
}

bool UCLLobbySubsystem::QueuePlan(const FGuid& SeatId, const TArray<FCLAgentStep>& Steps, bool bRemainder, FString& OutError)
{
	UCLParticipantSeat* Seat = FindSeat(SeatId);
	UCLRemoteAgentSeatMotor* Remote = Seat ? Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()) : nullptr;
	if (!Remote)
	{
		OutError = TEXT("not_remote_agent");
		return false;
	}
	return Remote->QueuePlan(Steps, bRemainder, OutError);
}

bool UCLLobbySubsystem::SetViewSeat(const FGuid& SeatId, FString& OutError)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!PC)
	{
		OutError = TEXT("no_pc");
		return false;
	}

	APawn* Target = SeatId.IsValid() ? GetDrivenPawn(SeatId) : nullptr;
	if (!Target)
	{
		Target = FindHumanPawn();
	}
	if (!Target)
	{
		OutError = TEXT("no_pawn");
		return false;
	}

	if (ACLPlayerCharacter* Prev = Cast<ACLPlayerCharacter>(LastDemoViewPawn.Get()))
	{
		if (Prev != Target)
		{
			Prev->SetDemoViewActive(false);
		}
	}
	if (ACLPlayerCharacter* Next = Cast<ACLPlayerCharacter>(Target))
	{
		Next->SetDemoViewActive(true);
	}
	LastDemoViewPawn = Target;

	FCLWeaponMotorTune Cam;
	Cam.LoadFromIni();
	PC->SetViewTargetWithBlend(Target, FMath::Max(0.05f, Cam.ViewBlendSeconds), VTBlend_EaseInOut, 2.f, false);
	OutError.Reset();
	return true;
}

bool UCLLobbySubsystem::StartGoto(const FGuid& SeatId, const FVector& Dest, FString& OutError)
{
	UCLParticipantSeat* Seat = FindSeat(SeatId);
	if (!Seat)
	{
		Seat = FindSeat(LastJoinedSeatId);
	}
	UCLRemoteAgentSeatMotor* Remote = Seat ? Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()) : nullptr;
	if (!Remote)
	{
		OutError = TEXT("not_remote_agent");
		return false;
	}
	ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn());
	if (!Char)
	{
		OutError = TEXT("no_driven_pawn");
		return false;
	}
	if (!Char->IsLocallyControlled())
	{
		OutError = TEXT("remote_player_pawn");
		return false;
	}
	return Remote->StartGoto(GetWorld(), Char, Dest, OutError);
}

void UCLLobbySubsystem::CheckMinPlayers()
{
	const FCLLobbyInvoice* Live = GetInvoice();
	if (!Live || !GateClock->IsUnlocked() || !GateClock->HasGate())
	{
		return;
	}
	if (SeatReg->Num() >= Live->MinPlayers)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (ACLPvpGameMode* Pvp = Cast<ACLPvpGameMode>(World->GetAuthGameMode()))
		{
			Pvp->EndMatchAndAward();
		}
	}
}

void UCLLobbySubsystem::PrepareGuestLocalHub(FGuid* FallbackSeat)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	UCLParticipantSeat* Human = EnsureNetHumanSeat(PC);
	UCLParticipantSeat* Cursor = nullptr;
	for (UCLParticipantSeat* Seat : SeatReg->GetAll())
	{
		if (Seat && Seat->GetSeatMotor() && Seat->GetSeatMotor()->IsA<UCLRemoteAgentSeatMotor>())
		{
			if (!Cursor)
			{
				Cursor = Seat;
			}
			if (Human && Seat->GetDrivenPawn() != Human->GetDrivenPawn())
			{
				FString Error;
				MindControl(Seat->GetSeatId(), Human->GetSeatId(), Error);
			}
		}
	}
	if (!Cursor)
	{
		FString Error;
		Cursor = JoinRemoteAgent(TEXT("guest-mcp"), true, Error, TEXT("cursor"));
	}
	if (Cursor && Human && Cursor->GetDrivenPawn() != Human->GetDrivenPawn())
	{
		FString Error;
		MindControl(Cursor->GetSeatId(), Human->GetSeatId(), Error);
	}
	if (Cursor)
	{
		LastJoinedSeatId = Cursor->GetSeatId();
		if (FallbackSeat)
		{
			*FallbackSeat = Cursor->GetSeatId();
		}
	}
}

bool UCLLobbySubsystem::TryRouteHubProxy(const TSharedPtr<FJsonObject>& Root, const FGuid& TargetInstance, const FGuid& ViaSeat, TFunction<void(FString)> OnDone)
{
	if (!Root.IsValid() || !OnDone)
	{
		return false;
	}
	auto Fail = [&OnDone](const TCHAR* Error)
	{
		OnDone(FString::Printf(TEXT("{\"ok\":false,\"error\":\"%s\"}"), Error));
	};
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			Fail(TEXT("cannot_proxy_here"));
			return true;
		}
	}
	FString Error;
	ACLPlayerController* CLPC = FindHubProxyTarget(TargetInstance, ViaSeat, Error);
	if (!CLPC)
	{
		if (Error.IsEmpty())
		{
			return false;
		}
		Fail(*Error);
		return true;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UCLInstanceIdentitySubsystem* Id = GI->GetSubsystem<UCLInstanceIdentitySubsystem>())
		{
			if (!JsonStr(Root, TEXT("originInstanceId")).Len())
			{
				Root->SetStringField(TEXT("originInstanceId"), GuidStr(Id->GetInstanceId()));
			}
		}
	}
	CLHubIngress::StripProxyFields(Root);
	const FString Payload = CLAgentCodec::JsonToString(Root.ToSharedRef());
	const int32 Id = NextHubViaId++;
	FHubViaPending Pending;
	Pending.OnDone = MoveTemp(OnDone);
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UCLLobbySubsystem> WeakThis(this);
		World->GetTimerManager().SetTimer(Pending.Timeout, [WeakThis, Id]()
		{
			if (UCLLobbySubsystem* Self = WeakThis.Get())
			{
				Self->CompleteHubVia(Id, TEXT("{\"ok\":false,\"error\":\"via_timeout\"}"));
			}
		}, 8.f, false);
	}
	HubViaPending.Add(Id, MoveTemp(Pending));
	UE_LOG(LogCallingHub, Display,
		TEXT("HubProxy targetInst=%s viaSeat=%s pc=%s"),
		*GuidStr(TargetInstance), *GuidStr(ViaSeat), *CLPC->GetName());
	CLPC->ClientHubDispatch(Payload, Id);
	return true;
}

ACLPlayerController* UCLLobbySubsystem::FindHubProxyTarget(const FGuid& TargetInstance, const FGuid& ViaSeat, FString& OutError) const
{
	OutError.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("no_proxy_target");
		return nullptr;
	}

	if (ViaSeat.IsValid())
	{
		if (const UCLParticipantSeat* Seat = FindSeat(ViaSeat))
		{
			if (APlayerController* PC = Seat->GetBoundController())
			{
				if (PC->IsLocalController())
				{
					return nullptr;
				}
				if (ACLPlayerController* CLPC = Cast<ACLPlayerController>(PC))
				{
					return CLPC;
				}
				OutError = TEXT("no_via_pc");
				return nullptr;
			}
			OutError = TEXT("no_via_controller");
			return nullptr;
		}
		OutError = TEXT("no_via_seat");
		return nullptr;
	}

	TArray<ACLPlayerController*> Remote;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		ACLPlayerController* CLPC = Cast<ACLPlayerController>(It->Get());
		if (!CLPC || CLPC->IsLocalController())
		{
			continue;
		}
		if (TargetInstance.IsValid())
		{
			if (CLPC->GetInstanceId() == TargetInstance)
			{
				return CLPC;
			}
			continue;
		}
		Remote.Add(CLPC);
	}
	if (TargetInstance.IsValid())
	{
		OutError = TEXT("no_proxy_target");
		return nullptr;
	}
	if (Remote.Num() == 1)
	{
		return Remote[0];
	}
	OutError = Remote.Num() == 0 ? TEXT("no_proxy_target") : TEXT("ambiguous_proxy_target");
	return nullptr;
}

void UCLLobbySubsystem::IngressLocalHub(const TSharedPtr<FJsonObject>& Root, FGuid* FallbackSeat, TFunction<void(FString)> OnDone)
{
	if (!OnDone)
	{
		return;
	}
	auto FailNow = [&OnDone](const FString& Error, UCLInstanceIdentitySubsystem* Id)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), Error);
		if (Id)
		{
			Id->StampJson(Out);
		}
		OnDone(CLAgentCodec::JsonToString(Out));
	};
	if (!Root.IsValid())
	{
		FailNow(TEXT("invalid_json"), nullptr);
		return;
	}
	UCLInstanceIdentitySubsystem* Id = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr;
	FString Error;
	if (Id && !Id->CheckInstance(Root, Error))
	{
		FailNow(Error, Id);
		return;
	}

	const TSharedRef<FJsonObject> Out = FCLHubCommandRegistry::Dispatch(this, Root, FallbackSeat);
	CLHubDriveTrace::ApplyToJson(Out);
	if (Id)
	{
		Id->StampJson(Out);
	}
	OnDone(CLAgentCodec::JsonToString(Out));
}

void UCLLobbySubsystem::HandleIncomingViaHub(const FString& Json, int32 CorrelationId, ACLPlayerController* ReplyTo)
{
	PrepareGuestLocalHub(nullptr);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FString Reply = TEXT("{\"ok\":false,\"error\":\"invalid_json\"}");
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		CLHubIngress::StripProxyFields(Root);
		CLHubDriveTrace::StampListen(Root, CLLoopbackJoin::AgentHttpPort(), TEXT("http"));
		if (UCLInstanceIdentitySubsystem* Id = GetGameInstance()->GetSubsystem<UCLInstanceIdentitySubsystem>())
		{
			Id->NoteJson(Root);
		}
		IngressLocalHub(Root, &LastJoinedSeatId, [&Reply](FString JsonOut)
		{
			Reply = MoveTemp(JsonOut);
		});
	}
	if (ReplyTo)
	{
		ReplyTo->ServerHubDispatchResult(CorrelationId, Reply);
	}
}

void UCLLobbySubsystem::CompleteHubVia(int32 CorrelationId, const FString& Json)
{
	FHubViaPending* Pending = HubViaPending.Find(CorrelationId);
	if (!Pending)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Pending->Timeout);
	}
	TFunction<void(FString)> Done = MoveTemp(Pending->OnDone);
	HubViaPending.Remove(CorrelationId);
	if (Done)
	{
		Done(Json);
	}
}

void UCLLobbySubsystem::TickNet(float DeltaSeconds)
{
	GateClock->TickCountdown(DeltaSeconds, [this]() { FinishGo(); });
	if (!GateClock->IsCountdownRunning() && GateClock->IsUnlocked())
	{
		CheckMinPlayers();
	}

	for (UCLParticipantSeat* Seat : SeatReg->GetAll())
	{
		if (Seat && Seat->GetSeatMotor())
		{
			Seat->GetSeatMotor()->TickNet(DeltaSeconds, Seat);
			if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
			{
				ECLHubSnapshotReason Reason = ECLHubSnapshotReason::Stale;
				while (Remote->ConsumePendingSnapshot(Reason))
				{
					if (Seat->GetSeatMotor()->WantsHubSnapshot(Reason))
					{
						NotifyHubSnapshots(Reason, Seat->GetSeatId());
					}
				}
			}
		}
	}
}

FString UCLLobbySubsystem::AccessName(ECLLobbyAccess Access)
{
	switch (Access)
	{
	case ECLLobbyAccess::Closed: return TEXT("closed");
	case ECLLobbyAccess::Party: return TEXT("party");
	case ECLLobbyAccess::Friends: return TEXT("friends");
	case ECLLobbyAccess::Guild: return TEXT("guild");
	default: return TEXT("open");
	}
}

FString UCLLobbySubsystem::TeamName(ECLPvpTeam Team)
{
	switch (Team)
	{
	case ECLPvpTeam::Blue: return TEXT("blue");
	case ECLPvpTeam::Red: return TEXT("red");
	default: return TEXT("unassigned");
	}
}

void UCLLobbySubsystem::NotifyHubSnapshots(ECLHubSnapshotReason Reason, const FGuid& OnlySeat)
{
	PushLobbyToGameState();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionHub* Hub = GI->GetSubsystem<UCLSessionHub>())
		{
			Hub->PushSnapshots(Reason, OnlySeat);
		}
	}
}

void UCLLobbySubsystem::FillStateJson(const TSharedRef<FJsonObject>& Root) const
{
	TSharedRef<FJsonObject> Lobby = MakeShared<FJsonObject>();
	Lobby->SetBoolField(TEXT("invoice"), HasInvoice());
	Lobby->SetBoolField(TEXT("gate"), HasGate());
	Lobby->SetBoolField(TEXT("unlocked"), IsGameplayUnlocked());
	Lobby->SetBoolField(TEXT("countdown"), IsCountdownRunning());
	Lobby->SetBoolField(TEXT("launchQueued"), IsMatchStartQueued());
	Lobby->SetBoolField(TEXT("readyLocked"), IsReadyLocked());
	if (const UCLParticipantSeat* Local = FindLocalSeat())
	{
		Lobby->SetBoolField(TEXT("localHost"), Local->IsHost());
	}
	Lobby->SetNumberField(TEXT("countdownLeft"), GetCountdownRemaining());
	Lobby->SetNumberField(TEXT("ready"), ReadyCount());
	Lobby->SetNumberField(TEXT("seats"), SeatReg->Num());
	Lobby->SetStringField(TEXT("realm"), GetLootRealmId().ToString());
	if (const FCLLobbyInvoice* Live = GetInvoice())
	{
		Lobby->SetNumberField(TEXT("minPlayers"), Live->MinPlayers);
		Lobby->SetNumberField(TEXT("maxPlayers"), Live->MaxPlayers);
		FString Access = AccessName(Live->Access);
		bool bListening = false;
		if (UWorld* World = GetWorld())
		{
			bListening = World->GetNetMode() == NM_ListenServer;
			if (!bListening && Live->Access == ECLLobbyAccess::Closed)
			{
				Access = TEXT("private");
			}
		}
		Lobby->SetStringField(TEXT("access"), Access);
		Lobby->SetBoolField(TEXT("listening"), bListening);
		if (UWorld* World = GetWorld())
		{
			const ENetMode Net = World->GetNetMode();
			Lobby->SetStringField(TEXT("netMode"),
				Net == NM_ListenServer ? TEXT("listen") : Net == NM_Client ? TEXT("client") : TEXT("standalone"));
		}
		if (!Live->GameModeId.IsNone())
		{
			Lobby->SetStringField(TEXT("gameMode"), Live->GameModeId.ToString());
		}
	}
	else if (UWorld* World = GetWorld())
	{
		const ENetMode Net = World->GetNetMode();
		Lobby->SetBoolField(TEXT("listening"), Net == NM_ListenServer);
		Lobby->SetStringField(TEXT("netMode"),
			Net == NM_ListenServer ? TEXT("listen") : Net == NM_Client ? TEXT("client") : TEXT("standalone"));
		if (Net == NM_Standalone)
		{
			Lobby->SetStringField(TEXT("access"), TEXT("private"));
		}
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (const UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Lobby->SetStringField(TEXT("socialKind"), FCLSocialDefault::KindToString(Sessions->GetLiveSocialKind()));
			const FString JoinFail = Sessions->GetJoinUnavailable();
			if (!JoinFail.IsEmpty())
			{
				Lobby->SetStringField(TEXT("joinUnavailable"), JoinFail);
			}
		}
		if (const UCLProfileSubsystem* Profiles = GI->GetSubsystem<UCLProfileSubsystem>())
		{
			const FCLSocialDefault Def = Profiles->GetSocialDefault();
			TSharedRef<FJsonObject> SocialDef = MakeShared<FJsonObject>();
			SocialDef->SetStringField(TEXT("kind"), FCLSocialDefault::KindToString(Def.Kind));
			SocialDef->SetStringField(TEXT("joinHost"), Def.JoinHost);
			SocialDef->SetNumberField(TEXT("joinPort"), Def.JoinPort);
			SocialDef->SetStringField(TEXT("joinFallback"), FCLSocialDefault::FallbackToString(Def.JoinFallback));
			Root->SetObjectField(TEXT("socialDefault"), SocialDef);
		}
	}
	if (LastJoinedSeatId.IsValid())
	{
		Lobby->SetStringField(TEXT("lastSeat"), GuidStr(LastJoinedSeatId));
	}

	TArray<TSharedPtr<FJsonValue>> SeatArr;
	for (const UCLParticipantSeat* Seat : SeatReg->GetAll())
	{
		if (!Seat)
		{
			continue;
		}
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("id"), GuidStr(Seat->GetSeatId()));
		Row->SetStringField(TEXT("name"), Seat->GetDisplayName());
		Row->SetStringField(TEXT("kind"), Seat->GetSeatMotor() ? Seat->GetSeatMotor()->GetKindId() : TEXT("none"));
		Row->SetBoolField(TEXT("ready"), Seat->IsReady());
		Row->SetBoolField(TEXT("host"), Seat->IsHost());
		Row->SetStringField(TEXT("team"), TeamName(Seat->GetTeam()));
		if (Seat->GetDriveSeatId().IsValid())
		{
			Row->SetStringField(TEXT("driveSeat"), GuidStr(Seat->GetDriveSeatId()));
		}
		if (Seat->GetRequestingAgentId().IsValid())
		{
			Row->SetStringField(TEXT("agentId"), GuidStr(Seat->GetRequestingAgentId()));
		}
		if (Seat->GetRequestorId().IsValid())
		{
			Row->SetStringField(TEXT("requestorId"), GuidStr(Seat->GetRequestorId()));
		}
		FGuid OwnerInst = Seat->GetOwnerInstanceId();
		bool bBoundLocal = false;
		if (APlayerController* Bound = Seat->GetBoundController())
		{
			bBoundLocal = Bound->IsLocalController();
			if (const ACLPlayerController* CLPC = Cast<ACLPlayerController>(Bound))
			{
				if (CLPC->GetInstanceId().IsValid())
				{
					OwnerInst = CLPC->GetInstanceId();
				}
			}
		}
		else if (const APawn* Driven = Seat->GetDrivenPawn())
		{
			bBoundLocal = Driven->IsLocallyControlled();
		}
		if (!OwnerInst.IsValid())
		{
			if (const UCLInstanceIdentitySubsystem* Id = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr)
			{
				OwnerInst = Id->GetInstanceId();
			}
		}
		if (OwnerInst.IsValid())
		{
			Row->SetStringField(TEXT("instanceId"), GuidStr(OwnerInst));
		}
		Row->SetBoolField(TEXT("boundLocal"), bBoundLocal);
		if (const UCLPossessionComponent* Possession = Seat->GetPossession())
		{
			const UEnum* Enum = StaticEnum<ECLPossessionMode>();
			Row->SetStringField(TEXT("possession"), Enum ? Enum->GetNameStringByValue(static_cast<int64>(Possession->GetMode())) : TEXT("Headless"));
		}
		if (const UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
		{
			Row->SetBoolField(TEXT("needsReplan"), Remote->NeedsReplan());
			Row->SetNumberField(TEXT("planLeft"), Remote->RemainingSeconds());
			if (Remote->IsGotoActive())
			{
				const FVector Goal = Remote->GetGotoGoal();
				Row->SetBoolField(TEXT("goto"), true);
				Row->SetNumberField(TEXT("gotoX"), Goal.X);
				Row->SetNumberField(TEXT("gotoY"), Goal.Y);
				Row->SetNumberField(TEXT("gotoZ"), Goal.Z);
			}
		}
		if (const UCLBotBookManager* Books = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLBotBookManager>() : nullptr)
		{
			Row->SetObjectField(TEXT("botBook"), Books->MakeSeatBotJson(Seat->GetSeatId()));
		}
		SeatArr.Add(MakeShared<FJsonValueObject>(Row));
	}
	Lobby->SetArrayField(TEXT("seatList"), SeatArr);
	Root->SetObjectField(TEXT("lobby"), Lobby);
}

TSharedRef<FJsonObject> UCLLobbySubsystem::HandleMessage(const TSharedPtr<FJsonObject>& Root)
{
	if (UCLInstanceIdentitySubsystem* Id = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr)
	{
		FString Error;
		if (!Id->CheckInstance(Root, Error))
		{
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
			Out->SetBoolField(TEXT("ok"), false);
			Out->SetStringField(TEXT("error"), Error);
			Id->StampJson(Out);
			return Out;
		}
	}
	const TSharedRef<FJsonObject> Out = FCLHubCommandRegistry::Dispatch(this, Root, nullptr);
	CLHubDriveTrace::ApplyToJson(Out);
	return Out;
}

UCLParticipantSeat* UCLLobbySubsystem::FindOrCreateLoopbackSeat()
{
	if (UCLParticipantSeat* Existing = FindSeat(LastJoinedSeatId))
	{
		if (Existing->GetSeatMotor() && Existing->GetSeatMotor()->IsA<UCLRemoteAgentSeatMotor>())
		{
			return Existing;
		}
	}
	for (UCLParticipantSeat* Seat : SeatReg->GetAll())
	{
		if (Seat && Seat->GetSeatMotor() && Seat->GetSeatMotor()->IsA<UCLRemoteAgentSeatMotor>())
		{
			LastJoinedSeatId = Seat->GetSeatId();
			return Seat;
		}
	}
	FString Error;
	return JoinRemoteAgent(TEXT("loopback"), false, Error, TEXT("cursor"));
}

void UCLLobbySubsystem::StampRosterOntoInvoice()
{
	Travel->StampRosterOntoInvoice(Invoices, SeatReg);
}

void UCLLobbySubsystem::RestoreBodiesAfterTravel()
{
	Travel->RestoreBodiesAfterTravel(Invoices, SeatReg, GateClock->GetGate());
}

