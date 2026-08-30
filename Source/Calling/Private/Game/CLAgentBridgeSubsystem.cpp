#include "Game/CLAgentBridgeSubsystem.h"
#include "Game/CLAgentCodec.h"
#include "Game/CLAgentStateSerializer.h"
#include "Game/CLDirectorCommandRegistry.h"
#include "Game/CLHubCommandRegistry.h"
#include "Game/CLHubDriveTrace.h"
#include "Game/CLHubIngress.h"
#include "Game/CLInstanceIdentity.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSeatMotor.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLPlayerController.h"
#include "Player/CLCombatPawn.h"
#include "Core/CLLog.h"
#include "Core/CLTickClock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpPath.h"
#include "HttpServerConstants.h"
#include "IPAddress.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Game/CLLoopbackJoin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"

namespace
{
	FString BodyToString(const TArray<uint8>& Body)
	{
		if (Body.Num() == 0)
		{
			return FString();
		}
		const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
		return FString(Conv.Length(), Conv.Get());
	}

	bool ParseJson(const FString& Body, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
	{
		OutRoot.Reset();
		if (Body.IsEmpty())
		{
			OutRoot = MakeShared<FJsonObject>();
			return true;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			OutError = TEXT("invalid_json");
			OutRoot = MakeShared<FJsonObject>();
			return false;
		}
		return true;
	}

	FString HttpHeader(const FHttpServerRequest& Request, const TCHAR* Name)
	{
		for (const TPair<FString, TArray<FString>>& Pair : Request.Headers)
		{
			if (Pair.Key.Equals(Name, ESearchCase::IgnoreCase) && Pair.Value.Num() > 0)
			{
				return Pair.Value[0];
			}
		}
		return FString();
	}

	void NoteAgentHttp(UGameInstance* GI, const FHttpServerRequest& Request, const TSharedPtr<FJsonObject>& Body, bool bMintIfMissing = true)
	{
		if (UCLInstanceIdentitySubsystem* Id = GI ? GI->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr)
		{
			FString Header = HttpHeader(Request, TEXT("X-Calling-Agent-Id"));
			if (Header.IsEmpty())
			{
				Header = HttpHeader(Request, TEXT("X-Calling-AgentId"));
			}
			FString Query;
			if (const FString* AgentId = Request.QueryParams.Find(TEXT("agentId")))
			{
				Query = *AgentId;
			}
			else if (const FString* Agent = Request.QueryParams.Find(TEXT("agent")))
			{
				Query = *Agent;
			}
			Id->NoteRequest(Header, Query, Body, bMintIfMissing);
		}
	}

	FString ReplyJson(UGameInstance* GI, const TSharedRef<FJsonObject>& Out)
	{
		if (const UCLInstanceIdentitySubsystem* Id = GI ? GI->GetSubsystem<UCLInstanceIdentitySubsystem>() : nullptr)
		{
			Id->StampJson(Out);
		}
		return CLAgentCodec::JsonToString(Out);
	}
}

void UCLAgentBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UCLTickSubsystem::StaticClass());
	Collection.InitializeDependency(UCLInstanceIdentitySubsystem::StaticClass());
	Super::Initialize(Collection);
#if UE_BUILD_SHIPPING
	bAllowAgentInput = false;
#else
	bAllowAgentInput = true;
	GConfig->GetBool(TEXT("/Script/Calling.CLAgentSettings"), TEXT("bAllowAgentInput"), bAllowAgentInput, GGameIni);
	Port = static_cast<uint32>(CLLoopbackJoin::AgentHttpPort());
	if (bAllowAgentInput)
	{
		StartListener();
	}
#endif
}

void UCLAgentBridgeSubsystem::Deinitialize()
{
	StopListener();
	Super::Deinitialize();
}

void UCLAgentBridgeSubsystem::StartListener()
{
	if (!FHttpServerModule::IsAvailable())
	{
		FModuleManager::LoadModuleChecked<FHttpServerModule>(TEXT("HTTPServer"));
	}
	FHttpServerModule& Http = FHttpServerModule::Get();
	Router = Http.GetHttpRouter(Port, true);
	if (!Router.IsValid())
	{
		UE_LOG(LogCalling, Warning, TEXT("Calling: agent HTTP failed to bind 127.0.0.1:%u"), Port);
		CLLoopbackJoin::AppendLog(FString::Printf(TEXT("http bind fail %u"), Port));
		return;
	}

	StateRoute = Router->BindRoute(FHttpPath(TEXT("/state")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleState));
	IntentRoute = Router->BindRoute(FHttpPath(TEXT("/intent")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleIntent));
	SequenceRoute = Router->BindRoute(FHttpPath(TEXT("/sequence")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleSequence));
	GotoRoute = Router->BindRoute(FHttpPath(TEXT("/goto")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleGoto));
	RespawnRoute = Router->BindRoute(FHttpPath(TEXT("/respawn")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleRespawn));
	DirectorRoute = Router->BindRoute(FHttpPath(TEXT("/director")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleDirector));
	HubRoute = Router->BindRoute(FHttpPath(TEXT("/hub")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UCLAgentBridgeSubsystem::HandleHub));
	Http.StartAllListeners();
	UE_LOG(LogCalling, Display, TEXT("Calling: agent HTTP on 127.0.0.1:%u (GET /state, POST /intent /sequence /goto /respawn /director /hub)"), Port);
}

void UCLAgentBridgeSubsystem::StopListener()
{
	if (Router.IsValid())
	{
		if (StateRoute.IsValid()) { Router->UnbindRoute(StateRoute); }
		if (IntentRoute.IsValid()) { Router->UnbindRoute(IntentRoute); }
		if (SequenceRoute.IsValid()) { Router->UnbindRoute(SequenceRoute); }
		if (GotoRoute.IsValid()) { Router->UnbindRoute(GotoRoute); }
		if (RespawnRoute.IsValid()) { Router->UnbindRoute(RespawnRoute); }
		if (DirectorRoute.IsValid()) { Router->UnbindRoute(DirectorRoute); }
		if (HubRoute.IsValid()) { Router->UnbindRoute(HubRoute); }
	}
	StateRoute.Reset();
	IntentRoute.Reset();
	SequenceRoute.Reset();
	GotoRoute.Reset();
	RespawnRoute.Reset();
	DirectorRoute.Reset();
	HubRoute.Reset();
	Router.Reset();
}

bool UCLAgentBridgeSubsystem::IsLoopback(const FHttpServerRequest& Request) const
{
	if (!Request.PeerAddress.IsValid())
	{
		return false;
	}
	const FString Host = Request.PeerAddress->ToString(false);
	return Host.Equals(TEXT("127.0.0.1"))
		|| Host.Equals(TEXT("::1"))
		|| Host.StartsWith(TEXT("127."))
		|| Host.Contains(TEXT(":127.0.0.1"));
}

UWorld* UCLAgentBridgeSubsystem::GetWorldSafe() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

UCLLobbySubsystem* UCLAgentBridgeSubsystem::GetLobby() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr;
}

UCLRemoteAgentSeatMotor* UCLAgentBridgeSubsystem::ResolveMotor(FGuid& InOutSeatId) const
{
	UCLLobbySubsystem* Lobby = GetLobby();
	if (!Lobby)
	{
		return nullptr;
	}
	if (!InOutSeatId.IsValid())
	{
		InOutSeatId = AgentSeatId.IsValid() ? AgentSeatId : Lobby->GetLastJoinedSeatId();
	}
	UCLParticipantSeat* Seat = Lobby->FindSeat(InOutSeatId);
	if (!Seat || !Seat->GetSeatMotor() || !Seat->GetSeatMotor()->IsA<UCLRemoteAgentSeatMotor>())
	{
		Seat = Lobby->FindOrCreateLoopbackSeat();
	}
	if (!Seat)
	{
		return nullptr;
	}
	InOutSeatId = Seat->GetSeatId();
	return Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor());
}

ACLPlayerCharacter* UCLAgentBridgeSubsystem::FindLocalPawn() const
{
	return ResolvePawn();
}

ACLPlayerCharacter* UCLAgentBridgeSubsystem::ResolvePawnForSeat(const FGuid& SeatId) const
{
	if (!SeatId.IsValid())
	{
		return nullptr;
	}
	if (UCLLobbySubsystem* Lobby = GetLobby())
	{
		return Cast<ACLPlayerCharacter>(Lobby->GetDrivenPawn(SeatId));
	}
	return nullptr;
}

ACLPlayerCharacter* UCLAgentBridgeSubsystem::ResolvePawn() const
{
	if (ACLPlayerCharacter* SeatPawn = ResolvePawnForSeat(AgentSeatId))
	{
		return SeatPawn;
	}
	UWorld* World = GetWorldSafe();
	if (!World)
	{
		return nullptr;
	}
	if (UCLLobbySubsystem* Lobby = GetLobby())
	{
		if (ACLPlayerCharacter* Human = Lobby->FindHumanPawn())
		{
			return Human;
		}
	}
	for (TActorIterator<ACLPlayerCharacter> It(World); It; ++It)
	{
		ACLPlayerCharacter* Char = *It;
		if (Char && Char->IsLocallyControlled() && !Char->IsA<ACLCombatPawn>())
		{
			return Char;
		}
	}
	return nullptr;
}

APlayerController* UCLAgentBridgeSubsystem::FindLocalController() const
{
	UWorld* World = GetWorldSafe();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

TSharedRef<FJsonObject> UCLAgentBridgeSubsystem::BuildStateJson(const FGuid& SeatId) const
{
	const FGuid ProbeSeat = SeatId.IsValid() ? SeatId : AgentSeatId;
	ACLPlayerCharacter* Char = SeatId.IsValid() ? ResolvePawnForSeat(SeatId) : FindLocalPawn();
	const UCLRemoteAgentSeatMotor* Remote = nullptr;
	if (UCLLobbySubsystem* Lobby = GetLobby())
	{
		if (const UCLParticipantSeat* Seat = Lobby->FindSeat(ProbeSeat))
		{
			Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor());
		}
	}
	return FCLAgentStateSerializer::Build(GetGameInstance(), Char, FindLocalController(), Remote, AgentSeatId, ProbeSeat);
}

FString UCLAgentBridgeSubsystem::BuildStateJsonString(const FGuid& SeatId) const
{
	return ReplyJson(GetGameInstance(), BuildStateJson(SeatId));
}

bool UCLAgentBridgeSubsystem::HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}
	FGuid SeatQuery;
	if (const FString* SeatStr = Request.QueryParams.Find(TEXT("seat")))
	{
		FGuid::Parse(*SeatStr, SeatQuery);
	}
	TSharedPtr<FJsonObject> Probe = MakeShared<FJsonObject>();
	NoteAgentHttp(GetGameInstance(), Request, Probe, false);
	OnComplete(FHttpServerResponse::Create(BuildStateJsonString(SeatQuery), TEXT("application/json")));
	return true;
}

bool UCLAgentBridgeSubsystem::HandleIntent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}
	NoteAgentHttp(GetGameInstance(), Request, Root);

	FGuid SeatId;
	UCLRemoteAgentSeatMotor* Motor = ResolveMotor(SeatId);
	if (Motor)
	{
		Motor->CancelMotor();
	}

	ACLPlayerCharacter* Char = SeatId.IsValid() ? ResolvePawnForSeat(SeatId) : FindLocalPawn();
	if (!Char)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_local_pawn")));
		return true;
	}

	FGuid TrackSeat;
	FCLAgentIntent Intent = CLAgentCodec::IntentFromObject(Root, TrackSeat);
	if (TrackSeat.IsValid())
	{
		Char->SetLookTrackSeat(TrackSeat);
	}
	else if (const TSharedPtr<FJsonObject> LookObj = CLAgentCodec::JsonObj(Root, TEXT("look")))
	{
		Char->ApplyAgentLookCommand(CLAgentCodec::ParseLook(LookObj));
	}
	Char->ApplyAgentIntent(Intent);
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("ok"), true);
	OnComplete(FHttpServerResponse::Create(ReplyJson(GetGameInstance(), Out), TEXT("application/json")));
	return true;
}

bool UCLAgentBridgeSubsystem::HandleSequence(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}
	NoteAgentHttp(GetGameInstance(), Request, Root);

	UCLLobbySubsystem* Lobby = GetLobby();
	if (!Lobby)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_lobby")));
		return true;
	}

	TArray<FCLAgentStep> Steps;
	bool bRemainder = false;
	CLAgentCodec::ParseSteps(Root, Steps, bRemainder);
	FGuid SeatId = CLAgentCodec::ParseGuid(CLAgentCodec::JsonStr(Root, TEXT("seatId")));
	UCLRemoteAgentSeatMotor* Motor = ResolveMotor(SeatId);
	if (!Motor)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("not_remote_agent")));
		return true;
	}

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	if (Steps.Num() == 0 && !bRemainder)
	{
		Motor->CancelMotor();
		if (ACLPlayerCharacter* Char = ResolvePawnForSeat(SeatId))
		{
			Char->ClearAgentIntent();
		}
		Out->SetBoolField(TEXT("ok"), true);
		Out->SetNumberField(TEXT("queued"), 0);
		Out->SetNumberField(TEXT("seconds"), 0);
		OnComplete(FHttpServerResponse::Create(ReplyJson(GetGameInstance(), Out), TEXT("application/json")));
		return true;
	}

	FString Error;
	if (!Lobby->QueuePlan(SeatId, Steps, bRemainder, Error))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, *Error));
		return true;
	}

	Out->SetBoolField(TEXT("ok"), true);
	Out->SetNumberField(TEXT("queued"), Motor->GetQueuedStepCount());
	Out->SetNumberField(TEXT("seconds"), Motor->RemainingSeconds());
	OnComplete(FHttpServerResponse::Create(ReplyJson(GetGameInstance(), Out), TEXT("application/json")));
	return true;
}

bool UCLAgentBridgeSubsystem::HandleGoto(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}
	NoteAgentHttp(GetGameInstance(), Request, Root);

	UCLLobbySubsystem* Lobby = GetLobby();
	if (!Lobby)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_lobby")));
		return true;
	}
	if (!Lobby->IsGameplayUnlocked())
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("lobby_locked")));
		return true;
	}

	FGuid SeatId = CLAgentCodec::ParseGuid(CLAgentCodec::JsonStr(Root, TEXT("seatId")));
	UCLRemoteAgentSeatMotor* Motor = ResolveMotor(SeatId);
	if (!Motor)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("not_remote_agent")));
		return true;
	}

	FString Error;
	const FVector Dest(
		CLAgentCodec::JsonNum(Root, TEXT("x")),
		CLAgentCodec::JsonNum(Root, TEXT("y")),
		CLAgentCodec::JsonNum(Root, TEXT("z")));
	if (!Lobby->StartGoto(SeatId, Dest, Error))
	{
		const EHttpServerResponseCodes Code = Error == TEXT("no_driven_pawn")
			? EHttpServerResponseCodes::NotFound
			: EHttpServerResponseCodes::BadRequest;
		OnComplete(FHttpServerResponse::Error(Code, *Error));
		return true;
	}

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("ok"), true);
	const FVector Goal = Motor->GetGotoGoal();
	Out->SetNumberField(TEXT("x"), Goal.X);
	Out->SetNumberField(TEXT("y"), Goal.Y);
	Out->SetNumberField(TEXT("z"), Goal.Z);
	Out->SetNumberField(TEXT("waypoints"), Motor->GetGotoWaypointCount());
	Out->SetBoolField(TEXT("partial"), Motor->IsGotoPartial());
	OnComplete(FHttpServerResponse::Create(ReplyJson(GetGameInstance(), Out), TEXT("application/json")));
	return true;
}

bool UCLAgentBridgeSubsystem::HandleRespawn(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	TSharedPtr<FJsonObject> Probe = MakeShared<FJsonObject>();
	NoteAgentHttp(GetGameInstance(), Request, Probe);

	FGuid SeatId;
	if (UCLRemoteAgentSeatMotor* Motor = ResolveMotor(SeatId))
	{
		Motor->CancelMotor();
		if (ACLPlayerCharacter* Char = ResolvePawnForSeat(SeatId))
		{
			Char->ClearAgentIntent();
		}
	}

	APlayerController* PC = FindLocalController();
	UWorld* World = GetWorldSafe();
	ACLGameModeBase* GM = World ? World->GetAuthGameMode<ACLGameModeBase>() : nullptr;
	if (!PC || !GM)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_local_controller")));
		return true;
	}

	GM->RequestRespawn(PC);
	OnComplete(FHttpServerResponse::Create(BuildStateJsonString(), TEXT("application/json")));
	return true;
}

bool UCLAgentBridgeSubsystem::HandleDirector(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}
	NoteAgentHttp(GetGameInstance(), Request, Root);
	FString Action = CLAgentCodec::JsonStr(Root, TEXT("action")).ToLower();
	if (Action.IsEmpty())
	{
		Action = TEXT("toggle");
	}

	ECLSceneId Scene = ECLSceneId::Boot;
	if (UWorld* World = GetWorldSafe())
	{
		if (const ACLGameModeBase* GM = World->GetAuthGameMode<ACLGameModeBase>())
		{
			Scene = GM->GetSceneId();
		}
	}
	if (Scene == ECLSceneId::Boot)
	{
		if (UCLProfileSubsystem* Profiles = GetGameInstance()->GetSubsystem<UCLProfileSubsystem>())
		{
			if (!Profiles->EnsurePlayableProfile())
			{
				OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("profile_create_failed")));
				return true;
			}
		}
		if (UCLSceneRouter* Scenes = GetGameInstance()->GetSubsystem<UCLSceneRouter>())
		{
			Scenes->TravelToScene(ECLSceneId::Social);
		}
		TSharedRef<FJsonObject> Entered = MakeShared<FJsonObject>();
		Entered->SetBoolField(TEXT("ok"), true);
		Entered->SetStringField(TEXT("action"), TEXT("enter"));
		Entered->SetBoolField(TEXT("enteringSocial"), true);
		FCLAgentStateSerializer::FillSceneMenu(Entered, GetGameInstance(), FindLocalController());
		OnComplete(FHttpServerResponse::Create(ReplyJson(GetGameInstance(), Entered), TEXT("application/json")));
		return true;
	}

	ACLPlayerController* PC = Cast<ACLPlayerController>(FindLocalController());
	const TSharedRef<FJsonObject> Out = FCLDirectorCommandRegistry::Dispatch(GetGameInstance(), PC, Action, &AgentSeatId);
	FCLAgentStateSerializer::FillSceneMenu(Out, GetGameInstance(), PC);
	OnComplete(FHttpServerResponse::Create(ReplyJson(GetGameInstance(), Out), TEXT("application/json")));
	return true;
}

bool UCLAgentBridgeSubsystem::HandleHub(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	UCLLobbySubsystem* Lobby = GetLobby();
	if (!Lobby)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_lobby")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}
	NoteAgentHttp(GetGameInstance(), Request, Root);

	CLHubDriveTrace::StampListen(Root, static_cast<int32>(Port), TEXT("http"));
	const FCLHubConnect Connect = CLHubIngress::Parse(
		Root,
		HttpHeader(Request, TEXT("Calling-Connect-Mode")).IsEmpty()
			? HttpHeader(Request, TEXT("X-Calling-Connect-Mode"))
			: HttpHeader(Request, TEXT("Calling-Connect-Mode")),
		HttpHeader(Request, TEXT("Calling-Target-Instance")).IsEmpty()
			? HttpHeader(Request, TEXT("X-Calling-Target-Instance"))
			: HttpHeader(Request, TEXT("Calling-Target-Instance")),
		Request.QueryParams.Find(TEXT("connectMode")) ? *Request.QueryParams.Find(TEXT("connectMode")) : FString(),
		Request.QueryParams.Find(TEXT("targetInstance")) ? *Request.QueryParams.Find(TEXT("targetInstance")) : FString());
	if (Connect.bProxy)
	{
		if (Lobby->TryRouteHubProxy(Root, Connect.TargetInstance, Connect.ViaSeat, [OnComplete](FString Json)
		{
			OnComplete(FHttpServerResponse::Create(MoveTemp(Json), TEXT("application/json")));
		}))
		{
			return true;
		}
	}

	Lobby->IngressLocalHub(Root, &AgentSeatId, [this, OnComplete](FString Json)
	{
		OnComplete(FHttpServerResponse::Create(MoveTemp(Json), TEXT("application/json")));
	});
	return true;
}
