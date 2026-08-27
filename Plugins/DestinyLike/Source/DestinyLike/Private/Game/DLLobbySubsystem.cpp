#include "Game/DLLobbySubsystem.h"
#include "Game/DLLobbyTypes.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLControllerPlaybook.h"
#include "Game/DLGameModeBase.h"
#include "Game/DLGameInstance.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLSessionHub.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLCombatPawn.h"
#include "Player/DLPossessionComponent.h"
#include "Player/DLHeadlessAgent.h"
#include "AI/DLSeatController.h"
#include "Core/DLTickClock.h"
#include "Core/DLLog.h"
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
#include "Core/DLTunes.h"

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

void UDLLobbySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UDLTickSubsystem::StaticClass());
	Super::Initialize(Collection);
	LoadCountdownFromConfig();
	BindClock();
}

void UDLLobbySubsystem::Deinitialize()
{
	UnbindClock();
	ClearScene();
	Super::Deinitialize();
}

void UDLLobbySubsystem::BindClock()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLTickSubsystem* Tick = GI->GetSubsystem<UDLTickSubsystem>())
		{
			NetTickHandle = Tick->OnNetTick().AddUObject(this, &UDLLobbySubsystem::TickNet);
		}
	}
}

void UDLLobbySubsystem::UnbindClock()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLTickSubsystem* Tick = GI->GetSubsystem<UDLTickSubsystem>())
		{
			Tick->OnNetTick().Remove(NetTickHandle);
		}
	}
	NetTickHandle = FDelegateHandle();
}

void UDLLobbySubsystem::SetPendingInvoice(const FDLLobbyInvoice& InInvoice)
{
	PendingInvoice = NewObject<UDLInvoiceBox>(this);
	PendingInvoice->Value = InInvoice;
}

void UDLLobbySubsystem::ClearPendingInvoice()
{
	PendingInvoice = nullptr;
}

const FDLLobbyInvoice* UDLLobbySubsystem::GetPendingInvoice() const
{
	return PendingInvoice ? &PendingInvoice->Value : nullptr;
}

const FDLLobbyInvoice* UDLLobbySubsystem::GetInvoice() const
{
	return Invoice ? &Invoice->Value : nullptr;
}

const FDLLobbyGate* UDLLobbySubsystem::GetGate() const
{
	return Gate ? &Gate->Value : nullptr;
}

FName UDLLobbySubsystem::GetLootRealmId() const
{
	if (const FDLLobbyInvoice* Live = GetInvoice())
	{
		return Live->LootRealm.RealmId.IsNone() ? FName(TEXT("local")) : Live->LootRealm.RealmId;
	}
	return FName(TEXT("local"));
}

void UDLLobbySubsystem::ConsumePendingOrDefault(EDLSceneId Scene)
{
	if (PendingInvoice)
	{
		Invoice = PendingInvoice;
		PendingInvoice = nullptr;
		return;
	}
	int32 MaxPlayers = 8;
	if (Scene == EDLSceneId::Pvp)
	{
		GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
		Invoice = NewObject<UDLInvoiceBox>(this);
		Invoice->Value = FDLLobbyInvoice::MakePvp(1, MaxPlayers);
	}
	else if (Scene == EDLSceneId::Composer)
	{
		GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Pvp"), MaxPlayers, GGameIni);
		Invoice = NewObject<UDLInvoiceBox>(this);
		Invoice->Value = FDLLobbyInvoice::MakeComposerPvp(2, MaxPlayers);
	}
	else if (Scene == EDLSceneId::Raid)
	{
		GConfig->GetInt(TEXT("/Script/DestinyLike.DLSessionSettings"), TEXT("MaxLobbyPlayers_Raid"), MaxPlayers, GGameIni);
		Invoice = NewObject<UDLInvoiceBox>(this);
		Invoice->Value = FDLLobbyInvoice::MakeRaid(0, 1, MaxPlayers);
	}
}

void UDLLobbySubsystem::LoadCountdownFromConfig()
{
	LaunchCountdownSeconds = 1.f;
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLLobbySettings"), TEXT("CountdownSeconds"), LaunchCountdownSeconds, GGameIni);
	LaunchCountdownSeconds = FMath::Max(0.2f, LaunchCountdownSeconds);
}

void UDLLobbySubsystem::InstallGateFromConfig()
{
	Gate = NewObject<UDLGateBox>(this);
	float Countdown = 1.f;
	float Stale = 3.f;
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLLobbySettings"), TEXT("CountdownSeconds"), Countdown, GGameIni);
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLLobbySettings"), TEXT("PlanStaleSeconds"), Stale, GGameIni);
	float Lookahead = 0.75f;
	GConfig->GetFloat(TEXT("/Script/DestinyLike.DLLobbySettings"), TEXT("PlanLookaheadSeconds"), Lookahead, GGameIni);
	Gate->Value.CountdownSeconds = FMath::Max(0.2f, Countdown);
	Gate->Value.PlanStaleSeconds = FMath::Max(0.25f, Stale);
	Gate->Value.PlanLookaheadSeconds = FMath::Max(0.1f, Lookahead);
}

void UDLLobbySubsystem::ClearScene()
{
	Seats.Reset();
	Invoice = nullptr;
	Gate = nullptr;
	bGameplayUnlocked = true;
	bCountdownRunning = false;
	bMatchStartQueued = false;
	CountdownRemaining = 0.f;
	LastJoinedSeatId.Invalidate();
}

void UDLLobbySubsystem::BeginOpenScene()
{
	Seats.Reset();
	Gate = nullptr;
	bGameplayUnlocked = true;
	bCountdownRunning = false;
	bMatchStartQueued = false;
	CountdownRemaining = 0.f;
	if (PendingInvoice)
	{
		Invoice = PendingInvoice;
		PendingInvoice = nullptr;
	}
	EnsureLocalHumanSeat();
}

void UDLLobbySubsystem::BeginGatedScene(EDLSceneId Scene)
{
	Seats.Reset();
	bGameplayUnlocked = false;
	bCountdownRunning = false;
	bMatchStartQueued = false;
	CountdownRemaining = 0.f;
	ConsumePendingOrDefault(Scene);
	InstallGateFromConfig();
	if (UDLParticipantSeat* Host = EnsureLocalHumanSeat())
	{
		Host->SetReady(true);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionHub* Hub = GI->GetSubsystem<UDLSessionHub>())
		{
			Hub->StartHost();
		}
	}
	StartCountdownIfReady();
}

void UDLLobbySubsystem::BeginComposerScene()
{
	Seats.Reset();
	Gate = nullptr;
	bGameplayUnlocked = true;
	bCountdownRunning = false;
	bMatchStartQueued = false;
	CountdownRemaining = 0.f;
	if (PendingInvoice)
	{
		Invoice = PendingInvoice;
		PendingInvoice = nullptr;
	}
	else
	{
		ConsumePendingOrDefault(EDLSceneId::Composer);
	}
	if (UDLParticipantSeat* Host = EnsureLocalHumanSeat())
	{
		Host->SetReady(false);
		if (Host->GetTeam() == EDLPvpTeam::Unassigned)
		{
			Host->SetTeam(EDLPvpTeam::Red);
		}
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionHub* Hub = GI->GetSubsystem<UDLSessionHub>())
		{
			Hub->StartHost();
		}
	}
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
}

void UDLLobbySubsystem::BeginPvpOrRestore()
{
	if (PendingInvoice)
	{
		Invoice = PendingInvoice;
		PendingInvoice = nullptr;
	}
	const bool bFromComposer = Seats.Num() > 0 && Invoice && Invoice->Value.Roster.Num() > 0;
	if (bFromComposer)
	{
		Invoice->Value.Activity = EDLSceneId::Pvp;
		Gate = nullptr;
		bGameplayUnlocked = true;
		bCountdownRunning = false;
		bMatchStartQueued = false;
		CountdownRemaining = 0.f;
		RestoreBodiesAfterTravel();
		NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
		return;
	}
	BeginGatedScene(EDLSceneId::Pvp);
}

UDLParticipantSeat* UDLLobbySubsystem::MakeSeat(const FString& DisplayName, UClass* PlaybookClass, const FGuid& ExistingId)
{
	UDLParticipantSeat* Seat = NewObject<UDLParticipantSeat>(this);
	UDLControllerPlaybook* Book = NewObject<UDLControllerPlaybook>(Seat, PlaybookClass);
	Seat->Configure(ExistingId.IsValid() ? ExistingId : FGuid::NewGuid(), DisplayName, Book);
	if (UDLRemoteAgentPlaybook* Remote = Cast<UDLRemoteAgentPlaybook>(Book))
	{
		if (const FDLLobbyGate* LiveGate = GetGate())
		{
			Remote->SetStaleSeconds(LiveGate->PlanStaleSeconds);
			Remote->SetLookaheadSeconds(LiveGate->PlanLookaheadSeconds);
		}
		else
		{
			float Stale = 3.f;
			float Lookahead = 0.75f;
			GConfig->GetFloat(TEXT("/Script/DestinyLike.DLLobbySettings"), TEXT("PlanStaleSeconds"), Stale, GGameIni);
			GConfig->GetFloat(TEXT("/Script/DestinyLike.DLLobbySettings"), TEXT("PlanLookaheadSeconds"), Lookahead, GGameIni);
			Remote->SetStaleSeconds(Stale);
			Remote->SetLookaheadSeconds(Lookahead);
		}
	}
	Seats.Add(Seat);
	return Seat;
}

UDLParticipantSeat* UDLLobbySubsystem::EnsureLocalHumanSeat()
{
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->IsHost())
		{
			return Seat;
		}
	}

	FString Name = TEXT("Host");
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLProfileSubsystem* Profiles = GI->GetSubsystem<UDLProfileSubsystem>())
		{
			const FString ProfileName = Profiles->GetActiveProfile().DisplayName;
			if (!ProfileName.IsEmpty())
			{
				Name = ProfileName;
			}
		}
	}

	UDLParticipantSeat* Seat = MakeSeat(Name, UDLHumanPlaybook::StaticClass());
	Seat->SetHost(true);
	if (ADLPlayerCharacter* Pawn = FindHumanPawn())
	{
		if (UDLPossessionComponent* Possession = Pawn->GetPossession())
		{
			Possession->PossessOwn(Pawn);
			Seat->SetPossession(Possession);
			Seat->SetAnchor(Pawn);
		}
	}
	return Seat;
}

ADLPlayerCharacter* UDLLobbySubsystem::FindHumanPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADLPlayerCharacter> It(World); It; ++It)
	{
		ADLPlayerCharacter* Char = *It;
		if (Char && Char->IsLocallyControlled() && !Char->IsA<ADLCombatPawn>())
		{
			return Char;
		}
	}
	return nullptr;
}

APawn* UDLLobbySubsystem::SpawnAgentPawn(EDLPvpTeam Team) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FVector Loc = FVector(0.f, 250.f, 130.f);
	FRotator Rot = FRotator::ZeroRotator;
	if (AActor* Start = FindTeamPlayerStart(Team))
	{
		Loc = Start->GetActorLocation();
		Rot = Start->GetActorRotation();
		if (const APlayerStart* Ps = Cast<APlayerStart>(Start))
		{
			if (Ps->PlayerStartTag.IsNone())
			{
				Loc += FVector(0.f, 280.f, 0.f);
			}
		}
	}
	else
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Loc = It->GetActorLocation() + FVector(0.f, 280.f, 0.f);
			Rot = It->GetActorRotation();
			break;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FTransform Xform(Rot, Loc);
	ADLCombatPawn* Pawn = World->SpawnActorDeferred<ADLCombatPawn>(
		ADLCombatPawn::StaticClass(), Xform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Pawn)
	{
		return nullptr;
	}
	Pawn->AutoPossessAI = EAutoPossessAI::Disabled;
	Pawn->AIControllerClass = nullptr;
	UGameplayStatics::FinishSpawningActor(Pawn, Xform);
	if (AController* Existing = Pawn->GetController())
	{
		Existing->UnPossess();
		Existing->Destroy();
	}
	ADLSeatController* Ctrl = World->SpawnActor<ADLSeatController>(ADLSeatController::StaticClass(), Loc, Rot, Params);
	if (Ctrl)
	{
		Ctrl->Possess(Pawn);
		Ctrl->SetControlRotation(Rot);
	}
	return Pawn;
}

UDLParticipantSeat* UDLLobbySubsystem::JoinRemoteAgent(const FString& DisplayName, bool bHeadless, FString& OutError, const FString& Kind)
{
	if (const FDLLobbyInvoice* Live = GetInvoice())
	{
		if (Seats.Num() >= Live->MaxPlayers)
		{
			OutError = TEXT("lobby_full");
			return nullptr;
		}
	}

	const FString Name = DisplayName.IsEmpty() ? TEXT("agent") : DisplayName;
	const FString UseKind = Kind.IsEmpty() ? TEXT("remoteAgent") : Kind;
	UClass* PlaybookClass = PlaybookClassFromKind(UseKind);
	if (!PlaybookClass || !PlaybookClass->IsChildOf(UDLRemoteAgentPlaybook::StaticClass()))
	{
		OutError = TEXT("not_remote_kind");
		return nullptr;
	}
	UDLParticipantSeat* Seat = MakeSeat(Name, PlaybookClass);
	UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("no_world");
		Seats.Remove(Seat);
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (bHeadless)
	{
		ADLHeadlessAgent* Anchor = World->SpawnActor<ADLHeadlessAgent>(ADLHeadlessAgent::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Anchor)
		{
			OutError = TEXT("spawn_failed");
			Seats.Remove(Seat);
			return nullptr;
		}
		Seat->SetAnchor(Anchor);
		Seat->SetPossession(Anchor->GetPossession());
		Seat->SetHeadlessJoin(true);
		Anchor->GetPossession()->GoHeadless();
		if (APawn* Body = SpawnAgentPawn(Seat->GetTeam()))
		{
			Anchor->GetPossession()->MindControl(Body);
			Seat->SetDriveSeatId(Seat->GetSeatId());
		}
	}
	else
	{
		APawn* Pawn = SpawnAgentPawn(Seat->GetTeam());
		ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Pawn);
		if (!Char || !Char->GetPossession())
		{
			OutError = TEXT("spawn_failed");
			Seats.Remove(Seat);
			return nullptr;
		}
		Char->GetPossession()->PossessOwn(Char);
		Seat->SetPossession(Char->GetPossession());
		Seat->SetAnchor(Char);
		Seat->SetHeadlessJoin(false);
		Seat->SetDriveSeatId(Seat->GetSeatId());
	}

	LastJoinedSeatId = Seat->GetSeatId();
	OutError.Reset();
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike: agent seat %s kind=%s headless=%d"),
		*GuidStr(Seat->GetSeatId()), *UseKind, bHeadless ? 1 : 0);
	return Seat;
}

UDLParticipantSeat* UDLLobbySubsystem::FindSeat(const FGuid& SeatId) const
{
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetSeatId() == SeatId)
		{
			return Seat;
		}
	}
	return nullptr;
}

UDLParticipantSeat* UDLLobbySubsystem::FindSeatByName(const FString& DisplayName) const
{
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetDisplayName().Equals(DisplayName, ESearchCase::IgnoreCase))
		{
			return Seat;
		}
	}
	return nullptr;
}

UDLParticipantSeat* UDLLobbySubsystem::FindHostSeat() const
{
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->IsHost())
		{
			return Seat;
		}
	}
	return nullptr;
}

UDLParticipantSeat* UDLLobbySubsystem::FindLocalSeat() const
{
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetPlaybook() && Seat->GetPlaybook()->IsA<UDLHumanPlaybook>())
		{
			return Seat;
		}
	}
	return FindHostSeat();
}

UDLParticipantSeat* UDLLobbySubsystem::FindSeatForController(const AController* Controller) const
{
	if (!Controller)
	{
		return nullptr;
	}
	if (Cast<APlayerController>(Controller))
	{
		if (UDLParticipantSeat* Local = FindLocalSeat())
		{
			return Local;
		}
		return FindHostSeat();
	}
	const APawn* Pawn = Controller->GetPawn();
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat)
		{
			continue;
		}
		if (Seat->GetDrivenPawn() == Pawn || Seat->GetAnchor() == Controller)
		{
			return Seat;
		}
	}
	return nullptr;
}

APawn* UDLLobbySubsystem::GetDrivenPawn(const FGuid& SeatId) const
{
	const UDLParticipantSeat* Seat = FindSeat(SeatId);
	return Seat ? Seat->GetDrivenPawn() : nullptr;
}

bool UDLLobbySubsystem::IsRemotelyDriven(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	for (const UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat || !Seat->GetPossession() || !Seat->GetPossession()->Drives(Pawn))
		{
			continue;
		}
		if (Seat->GetPlaybook() && Seat->GetPlaybook()->IsA<UDLRemoteAgentPlaybook>())
		{
			return Seat->GetPossession()->GetMode() == EDLPossessionMode::MindControl
				|| !Pawn->IsLocallyControlled();
		}
	}
	return false;
}

int32 UDLLobbySubsystem::ReadyCount() const
{
	int32 Count = 0;
	for (const UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->IsReady())
		{
			++Count;
		}
	}
	return Count;
}

TArray<UDLParticipantSeat*> UDLLobbySubsystem::GetSeats() const
{
	TArray<UDLParticipantSeat*> Out;
	for (const TObjectPtr<UDLParticipantSeat>& Seat : Seats)
	{
		if (Seat)
		{
			Out.Add(Seat.Get());
		}
	}
	return Out;
}

bool UDLLobbySubsystem::IsReadyLocked() const
{
	return bMatchStartQueued || (Gate != nullptr && bGameplayUnlocked);
}

bool UDLLobbySubsystem::SetReady(const FGuid& SeatId, bool bReady)
{
	if (IsReadyLocked())
	{
		return false;
	}
	UDLParticipantSeat* Seat = FindSeat(SeatId);
	if (!Seat)
	{
		if (UDLParticipantSeat* Host = EnsureLocalHumanSeat())
		{
			Seat = Host;
		}
	}
	if (!Seat)
	{
		return false;
	}
	Seat->SetReady(bReady);
	if (!bReady && !bMatchStartQueued)
	{
		bCountdownRunning = false;
		CountdownRemaining = 0.f;
	}
	if (Gate && !bGameplayUnlocked)
	{
		StartCountdownIfReady();
	}
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
	return true;
}

bool UDLLobbySubsystem::ToggleLocalReady()
{
	UDLParticipantSeat* Local = FindLocalSeat();
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

bool UDLLobbySubsystem::ClaimLocalHost()
{
	UDLParticipantSeat* Local = EnsureLocalHumanSeat();
	if (!Local)
	{
		return false;
	}
	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat != Local)
		{
			Seat->SetHost(false);
		}
	}
	Local->SetHost(true);
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
	return true;
}

bool UDLLobbySubsystem::ClaimLocalGuest()
{
	UDLParticipantSeat* Local = EnsureLocalHumanSeat();
	if (!Local)
	{
		return false;
	}
	Local->SetHost(false);
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
	return true;
}

bool UDLLobbySubsystem::SetTeam(const FGuid& SeatId, EDLPvpTeam Team, FString& OutError)
{
	UDLParticipantSeat* Seat = FindSeat(SeatId);
	if (!Seat)
	{
		OutError = TEXT("no_seat");
		return false;
	}
	Seat->SetTeam(Team);
	OutError.Reset();
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
	return true;
}

void UDLLobbySubsystem::StartCountdownIfReady()
{
	const FDLLobbyInvoice* Live = GetInvoice();
	const FDLLobbyGate* LiveGate = GetGate();
	if (!Live || !LiveGate || bGameplayUnlocked || bMatchStartQueued)
	{
		return;
	}
	if (ReadyCount() < Live->MinPlayers)
	{
		bCountdownRunning = false;
		CountdownRemaining = 0.f;
		return;
	}
	if (!bCountdownRunning)
	{
		bCountdownRunning = true;
		CountdownRemaining = LiveGate->CountdownSeconds;
	}
}

bool UDLLobbySubsystem::RequestLocalGo()
{
	const UDLParticipantSeat* Local = FindLocalSeat();
	if (!Local || !Local->IsHost())
	{
		return false;
	}
	return RequestGo();
}

bool UDLLobbySubsystem::RequestGo()
{
	UDLParticipantSeat* Host = FindHostSeat();
	if (!Host)
	{
		return false;
	}
	const FDLLobbyInvoice* Live = GetInvoice();
	if (!Gate)
	{
		if (!Live || ReadyCount() < Live->MinPlayers)
		{
			return false;
		}
		bMatchStartQueued = true;
		if (!bCountdownRunning)
		{
			bCountdownRunning = true;
			CountdownRemaining = LaunchCountdownSeconds;
			NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
			return true;
		}
		if (CountdownRemaining > 0.f)
		{
			return true;
		}
		FinishGo();
		return true;
	}
	if (!Live || ReadyCount() < Live->MinPlayers)
	{
		return false;
	}
	bMatchStartQueued = true;
	FinishGo();
	return true;
}

void UDLLobbySubsystem::FinishGo()
{
	bCountdownRunning = false;
	CountdownRemaining = 0.f;
	bGameplayUnlocked = true;
	NotifyHubSnapshots(EDLHubSnapshotReason::LobbyDirty);
	if (UWorld* World = GetWorld())
	{
		if (ADLGameModeBase* GM = World->GetAuthGameMode<ADLGameModeBase>())
		{
			GM->HandleLobbyGo();
		}
	}
}

bool UDLLobbySubsystem::MindControl(const FGuid& AgentSeatId, const FGuid& TargetSeatId, FString& OutError)
{
	UDLParticipantSeat* Agent = FindSeat(AgentSeatId);
	UDLParticipantSeat* Target = FindSeat(TargetSeatId);
	if (!Agent || !Agent->GetPlaybook() || !Agent->GetPlaybook()->IsA<UDLRemoteAgentPlaybook>())
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
	Agent->GetPossession()->MindControl(TargetPawn);
	Agent->SetDriveSeatId(Target ? Target->GetSeatId() : FGuid());
	OutError.Reset();
	return true;
}

bool UDLLobbySubsystem::QueuePlan(const FGuid& SeatId, const TArray<FDLAgentStep>& Steps, bool bRemainder, FString& OutError)
{
	UDLParticipantSeat* Seat = FindSeat(SeatId);
	UDLRemoteAgentPlaybook* Remote = Seat ? Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook()) : nullptr;
	if (!Remote)
	{
		OutError = TEXT("not_remote_agent");
		return false;
	}
	return Remote->QueuePlan(Steps, bRemainder, OutError);
}

bool UDLLobbySubsystem::SetViewSeat(const FGuid& SeatId, FString& OutError)
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

	if (ADLPlayerCharacter* Prev = Cast<ADLPlayerCharacter>(LastDemoViewPawn.Get()))
	{
		if (Prev != Target)
		{
			Prev->SetDemoViewActive(false);
		}
	}
	if (ADLPlayerCharacter* Next = Cast<ADLPlayerCharacter>(Target))
	{
		Next->SetDemoViewActive(true);
	}
	LastDemoViewPawn = Target;

	FDLWeaponMotorTune Cam;
	Cam.LoadFromIni();
	PC->SetViewTargetWithBlend(Target, FMath::Max(0.05f, Cam.ViewBlendSeconds), VTBlend_EaseInOut, 2.f, false);
	OutError.Reset();
	return true;
}

bool UDLLobbySubsystem::StartGoto(const FGuid& SeatId, const FVector& Dest, FString& OutError)
{
	UDLParticipantSeat* Seat = FindSeat(SeatId);
	if (!Seat)
	{
		Seat = FindSeat(LastJoinedSeatId);
	}
	UDLRemoteAgentPlaybook* Remote = Seat ? Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook()) : nullptr;
	if (!Remote)
	{
		OutError = TEXT("not_remote_agent");
		return false;
	}
	ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Seat->GetDrivenPawn());
	if (!Char)
	{
		OutError = TEXT("no_driven_pawn");
		return false;
	}
	return Remote->StartGoto(GetWorld(), Char, Dest, OutError);
}

void UDLLobbySubsystem::CheckMinPlayers()
{
	const FDLLobbyInvoice* Live = GetInvoice();
	if (!Live || !bGameplayUnlocked || !Gate)
	{
		return;
	}
	if (Seats.Num() >= Live->MinPlayers)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (ADLPvpGameMode* Pvp = Cast<ADLPvpGameMode>(World->GetAuthGameMode()))
		{
			Pvp->EndMatchAndAward();
		}
	}
}

void UDLLobbySubsystem::TickNet(float DeltaSeconds)
{
	if (bCountdownRunning)
	{
		CountdownRemaining -= DeltaSeconds;
		if (CountdownRemaining <= 0.f)
		{
			bCountdownRunning = false;
			CountdownRemaining = 0.f;
			if (bMatchStartQueued || Gate)
			{
				FinishGo();
			}
		}
	}
	else if (bGameplayUnlocked)
	{
		CheckMinPlayers();
	}

	for (UDLParticipantSeat* Seat : Seats)
	{
		if (Seat && Seat->GetPlaybook())
		{
			Seat->GetPlaybook()->TickNet(DeltaSeconds, Seat);
			if (UDLRemoteAgentPlaybook* Remote = Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook()))
			{
				EDLHubSnapshotReason Reason = EDLHubSnapshotReason::Stale;
				while (Remote->ConsumePendingSnapshot(Reason))
				{
					if (Seat->GetPlaybook()->WantsHubSnapshot(Reason))
					{
						NotifyHubSnapshots(Reason, Seat->GetSeatId());
					}
				}
			}
		}
	}
}

FString UDLLobbySubsystem::AccessName(EDLLobbyAccess Access)
{
	switch (Access)
	{
	case EDLLobbyAccess::Closed: return TEXT("closed");
	case EDLLobbyAccess::Party: return TEXT("party");
	case EDLLobbyAccess::Friends: return TEXT("friends");
	case EDLLobbyAccess::Guild: return TEXT("guild");
	default: return TEXT("open");
	}
}

FString UDLLobbySubsystem::TeamName(EDLPvpTeam Team)
{
	switch (Team)
	{
	case EDLPvpTeam::Blue: return TEXT("blue");
	case EDLPvpTeam::Red: return TEXT("red");
	default: return TEXT("unassigned");
	}
}

EDLPvpTeam UDLLobbySubsystem::ParseTeam(const FString& Text)
{
	const FString Lower = Text.ToLower();
	if (Lower == TEXT("blue"))
	{
		return EDLPvpTeam::Blue;
	}
	if (Lower == TEXT("red"))
	{
		return EDLPvpTeam::Red;
	}
	return EDLPvpTeam::Unassigned;
}

UClass* UDLLobbySubsystem::PlaybookClassFromKind(const FString& Kind)
{
	if (Kind == TEXT("cursor"))
	{
		return UDLCursorPlaybook::StaticClass();
	}
	if (Kind == TEXT("remoteAgent"))
	{
		return UDLRemoteAgentPlaybook::StaticClass();
	}
	if (Kind == TEXT("algorithmic"))
	{
		return UDLAlgorithmicPlaybook::StaticClass();
	}
	return UDLHumanPlaybook::StaticClass();
}

AActor* UDLLobbySubsystem::FindTeamPlayerStart(EDLPvpTeam Team) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	const FName Tag = Team == EDLPvpTeam::Blue ? FName(TEXT("Blue")) : FName(TEXT("Red"));
	AActor* Fallback = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->IsA<APlayerStartPIE>())
		{
			continue;
		}
		if (It->PlayerStartTag == Tag)
		{
			return *It;
		}
		if (!Fallback)
		{
			Fallback = *It;
		}
	}
	return Fallback;
}

void UDLLobbySubsystem::NotifyHubSnapshots(EDLHubSnapshotReason Reason, const FGuid& OnlySeat)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLSessionHub* Hub = GI->GetSubsystem<UDLSessionHub>())
		{
			Hub->PushSnapshots(Reason, OnlySeat);
		}
	}
}

void UDLLobbySubsystem::FillStateJson(const TSharedRef<FJsonObject>& Root) const
{
	TSharedRef<FJsonObject> Lobby = MakeShared<FJsonObject>();
	Lobby->SetBoolField(TEXT("invoice"), Invoice != nullptr);
	Lobby->SetBoolField(TEXT("gate"), Gate != nullptr);
	Lobby->SetBoolField(TEXT("unlocked"), bGameplayUnlocked);
	Lobby->SetBoolField(TEXT("countdown"), bCountdownRunning);
	Lobby->SetBoolField(TEXT("launchQueued"), bMatchStartQueued);
	Lobby->SetBoolField(TEXT("readyLocked"), IsReadyLocked());
	if (const UDLParticipantSeat* Local = FindLocalSeat())
	{
		Lobby->SetBoolField(TEXT("localHost"), Local->IsHost());
	}
	Lobby->SetNumberField(TEXT("countdownLeft"), CountdownRemaining);
	Lobby->SetNumberField(TEXT("ready"), ReadyCount());
	Lobby->SetNumberField(TEXT("seats"), Seats.Num());
	Lobby->SetStringField(TEXT("realm"), GetLootRealmId().ToString());
	if (const FDLLobbyInvoice* Live = GetInvoice())
	{
		Lobby->SetNumberField(TEXT("minPlayers"), Live->MinPlayers);
		Lobby->SetNumberField(TEXT("maxPlayers"), Live->MaxPlayers);
		Lobby->SetStringField(TEXT("access"), AccessName(Live->Access));
	}
	if (LastJoinedSeatId.IsValid())
	{
		Lobby->SetStringField(TEXT("lastSeat"), GuidStr(LastJoinedSeatId));
	}

	TArray<TSharedPtr<FJsonValue>> SeatArr;
	for (const UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat)
		{
			continue;
		}
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("id"), GuidStr(Seat->GetSeatId()));
		Row->SetStringField(TEXT("name"), Seat->GetDisplayName());
		Row->SetStringField(TEXT("kind"), Seat->GetPlaybook() ? Seat->GetPlaybook()->GetKindId() : TEXT("none"));
		Row->SetBoolField(TEXT("ready"), Seat->IsReady());
		Row->SetBoolField(TEXT("host"), Seat->IsHost());
		Row->SetStringField(TEXT("team"), TeamName(Seat->GetTeam()));
		if (Seat->GetDriveSeatId().IsValid())
		{
			Row->SetStringField(TEXT("driveSeat"), GuidStr(Seat->GetDriveSeatId()));
		}
		if (const UDLPossessionComponent* Possession = Seat->GetPossession())
		{
			const UEnum* Enum = StaticEnum<EDLPossessionMode>();
			Row->SetStringField(TEXT("possession"), Enum ? Enum->GetNameStringByValue(static_cast<int64>(Possession->GetMode())) : TEXT("Headless"));
		}
		if (const UDLRemoteAgentPlaybook* Remote = Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook()))
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
		SeatArr.Add(MakeShared<FJsonValueObject>(Row));
	}
	Lobby->SetArrayField(TEXT("seatList"), SeatArr);
	Root->SetObjectField(TEXT("lobby"), Lobby);
}

TSharedRef<FJsonObject> UDLLobbySubsystem::HandleMessage(const TSharedPtr<FJsonObject>& Root)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	const FString Type = JsonStr(Root, TEXT("type")).ToLower();
	if (Type.IsEmpty())
	{
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), TEXT("missing_type"));
		return Out;
	}

	if (Type == TEXT("join"))
	{
		FString Error;
		UDLParticipantSeat* Seat = JoinRemoteAgent(
			JsonStr(Root, TEXT("displayName"), TEXT("agent")),
			JsonBool(Root, TEXT("headless")),
			Error,
			JsonStr(Root, TEXT("kind")));
		Out->SetBoolField(TEXT("ok"), Seat != nullptr);
		if (Seat)
		{
			Out->SetStringField(TEXT("seatId"), GuidStr(Seat->GetSeatId()));
			Out->SetBoolField(TEXT("headless"), JsonBool(Root, TEXT("headless")));
			Out->SetStringField(TEXT("kind"), Seat->GetPlaybook() ? Seat->GetPlaybook()->GetKindId() : TEXT("none"));
		}
		else
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	if (Type == TEXT("subscribe"))
	{
		const FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		if (!FindSeat(SeatId))
		{
			Out->SetBoolField(TEXT("ok"), false);
			Out->SetStringField(TEXT("error"), TEXT("no_seat"));
			return Out;
		}
		Out->SetBoolField(TEXT("ok"), true);
		Out->SetStringField(TEXT("seatId"), GuidStr(SeatId));
		return Out;
	}

	if (Type == TEXT("ready"))
	{
		const FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		const bool bOk = SetReady(SeatId, JsonBool(Root, TEXT("ready"), true));
		Out->SetBoolField(TEXT("ok"), bOk);
		return Out;
	}

	if (Type == TEXT("go"))
	{
		const FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		const UDLParticipantSeat* Seat = SeatId.IsValid() ? FindSeat(SeatId) : FindLocalSeat();
		if (!Seat || !Seat->IsHost())
		{
			Out->SetBoolField(TEXT("ok"), false);
			Out->SetStringField(TEXT("error"), TEXT("host_only"));
			return Out;
		}
		Out->SetBoolField(TEXT("ok"), RequestGo());
		return Out;
	}

	if (Type == TEXT("mindcontrol"))
	{
		FString Error;
		const bool bOk = MindControl(ParseGuid(JsonStr(Root, TEXT("seatId"))), ParseGuid(JsonStr(Root, TEXT("targetSeatId"))), Error);
		Out->SetBoolField(TEXT("ok"), bOk);
		if (!bOk)
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	if (Type == TEXT("setteam"))
	{
		FString Error;
		FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		if (!SeatId.IsValid())
		{
			SeatId = LastJoinedSeatId;
		}
		const bool bOk = SetTeam(SeatId, ParseTeam(JsonStr(Root, TEXT("team"))), Error);
		Out->SetBoolField(TEXT("ok"), bOk);
		if (!bOk)
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	if (Type == TEXT("goto"))
	{
		FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		FString Error;
		const FVector Dest(JsonNum(Root, TEXT("x")), JsonNum(Root, TEXT("y")), JsonNum(Root, TEXT("z")));
		const bool bOk = StartGoto(SeatId, Dest, Error);
		Out->SetBoolField(TEXT("ok"), bOk);
		if (bOk)
		{
			UDLParticipantSeat* Seat = FindSeat(SeatId.IsValid() ? SeatId : LastJoinedSeatId);
			if (const UDLRemoteAgentPlaybook* Remote = Seat ? Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook()) : nullptr)
			{
				const FVector Goal = Remote->GetGotoGoal();
				Out->SetNumberField(TEXT("x"), Goal.X);
				Out->SetNumberField(TEXT("y"), Goal.Y);
				Out->SetNumberField(TEXT("z"), Goal.Z);
				Out->SetNumberField(TEXT("waypoints"), Remote->GetGotoWaypointCount());
				Out->SetBoolField(TEXT("partial"), Remote->IsGotoPartial());
			}
		}
		else
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	if (Type == TEXT("view"))
	{
		FString Error;
		FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		const bool bOk = SetViewSeat(SeatId, Error);
		Out->SetBoolField(TEXT("ok"), bOk);
		if (bOk)
		{
			Out->SetStringField(TEXT("seatId"), GuidStr(SeatId));
		}
		else
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	Out->SetBoolField(TEXT("ok"), false);
	Out->SetStringField(TEXT("error"), TEXT("unknown_type"));
	return Out;
}

void UDLLobbySubsystem::StampRosterOntoInvoice()
{
	if (!Invoice)
	{
		return;
	}
	Invoice->Value.Roster.Reset();
	Invoice->Value.Activity = EDLSceneId::Pvp;
	for (const UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat)
		{
			continue;
		}
		FDLInvoiceSeat Row;
		Row.SeatId = Seat->GetSeatId();
		Row.DisplayName = Seat->GetDisplayName();
		Row.Team = Seat->GetTeam();
		Row.Kind = Seat->GetPlaybook() ? Seat->GetPlaybook()->GetKindId() : TEXT("none");
		Row.DriveSeatId = Seat->GetDriveSeatId();
		Row.bHeadless = Seat->IsHeadlessJoin();
		Invoice->Value.Roster.Add(Row);
	}
	SetPendingInvoice(Invoice->Value);
}

void UDLLobbySubsystem::RecreateSeatsFromRoster()
{
	if (!Invoice || Invoice->Value.Roster.Num() == 0)
	{
		return;
	}
	Seats.Reset();
	for (const FDLInvoiceSeat& Row : Invoice->Value.Roster)
	{
		UDLParticipantSeat* Seat = MakeSeat(Row.DisplayName, PlaybookClassFromKind(Row.Kind), Row.SeatId);
		Seat->SetTeam(Row.Team);
		Seat->SetHeadlessJoin(Row.bHeadless);
		Seat->SetDriveSeatId(Row.DriveSeatId.IsValid() ? Row.DriveSeatId : Row.SeatId);
		if (Row.Kind == TEXT("human"))
		{
			Seat->SetHost(true);
		}
	}
}

void UDLLobbySubsystem::RestoreBodiesAfterTravel()
{
	if (Seats.Num() == 0)
	{
		RecreateSeatsFromRoster();
	}

	for (UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat)
		{
			continue;
		}
		if (UDLRemoteAgentPlaybook* Remote = Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook()))
		{
			Remote->CancelPlan();
			Remote->CancelGoto();
		}
		Seat->SetPossession(nullptr);
		Seat->SetAnchor(nullptr);
	}

	if (UDLParticipantSeat* Host = FindHostSeat())
	{
		if (ADLPlayerCharacter* Human = FindHumanPawn())
		{
			if (UDLPossessionComponent* Possession = Human->GetPossession())
			{
				Possession->PossessOwn(Human);
				Host->SetPossession(Possession);
				Host->SetAnchor(Human);
			}
			if (AActor* Start = FindTeamPlayerStart(Host->GetTeam()))
			{
				Human->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation(), false, true);
			}
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat || Seat->IsHost())
		{
			continue;
		}
		if (!Seat->GetPlaybook() || !Seat->GetPlaybook()->IsA<UDLRemoteAgentPlaybook>())
		{
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (Seat->IsHeadlessJoin())
		{
			ADLHeadlessAgent* Anchor = World->SpawnActor<ADLHeadlessAgent>(ADLHeadlessAgent::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (!Anchor)
			{
				continue;
			}
			Seat->SetAnchor(Anchor);
			Seat->SetPossession(Anchor->GetPossession());
			Anchor->GetPossession()->GoHeadless();
		}

		const FGuid DriveId = Seat->GetDriveSeatId();
		const bool bOwnBody = !DriveId.IsValid() || DriveId == Seat->GetSeatId();
		if (bOwnBody)
		{
			if (APawn* Body = SpawnAgentPawn(Seat->GetTeam()))
			{
				if (Seat->GetPossession())
				{
					Seat->GetPossession()->MindControl(Body);
				}
				else if (ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Body))
				{
					Char->GetPossession()->PossessOwn(Char);
					Seat->SetPossession(Char->GetPossession());
					Seat->SetAnchor(Char);
				}
				Seat->SetDriveSeatId(Seat->GetSeatId());
			}
		}
	}

	for (UDLParticipantSeat* Seat : Seats)
	{
		if (!Seat || Seat->IsHost() || !Seat->GetPossession())
		{
			continue;
		}
		const FGuid DriveId = Seat->GetDriveSeatId();
		if (!DriveId.IsValid() || DriveId == Seat->GetSeatId())
		{
			continue;
		}
		if (UDLParticipantSeat* Target = FindSeat(DriveId))
		{
			if (APawn* TargetPawn = Target->GetDrivenPawn())
			{
				Seat->GetPossession()->MindControl(TargetPawn);
			}
		}
	}
}
