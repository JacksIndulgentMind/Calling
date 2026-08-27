#include "Game/DLAgentBridgeSubsystem.h"
#include "Combat/DLHitscanService.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Combat/DLDamageableComponent.h"
#include "Player/DLWeaponMotorComponent.h"
#include "Core/DLTickClock.h"
#include "Game/DLGameModeBase.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLSceneRouter.h"
#include "Player/DLPlayerController.h"
#include "UI/DLMainMenuOverlay.h"
#include "Game/DLActivityLauncher.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLSessionHub.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLControllerPlaybook.h"
#include "Player/DLCombatPawn.h"
#include "Nav/DLAgentNavProbe.h"
#include "Nav/DLNavTune.h"
#include "Input/DLAgentIntent.h"
#include "Core/DLLog.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Modules/ModuleManager.h"
#include "Components/CapsuleComponent.h"

namespace
{
	const TCHAR* SceneName(EDLSceneId Scene)
	{
		switch (Scene)
		{
		case EDLSceneId::Social: return TEXT("social");
		case EDLSceneId::Composer: return TEXT("composer");
		case EDLSceneId::Pvp: return TEXT("pvp");
		case EDLSceneId::Raid: return TEXT("raid");
		case EDLSceneId::Practice: return TEXT("practice");
		default: return TEXT("boot");
		}
	}

	FString BodyToString(const TArray<uint8>& Body)
	{
		if (Body.Num() == 0)
		{
			return FString();
		}
		const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
		return FString(Conv.Length(), Conv.Get());
	}

	bool JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool bDefault = false)
	{
		if (!Obj.IsValid() || !Obj->HasField(Key))
		{
			return bDefault;
		}
		const TSharedPtr<FJsonValue> Val = Obj->TryGetField(Key);
		if (!Val.IsValid())
		{
			return bDefault;
		}
		if (Val->Type == EJson::Boolean)
		{
			return Val->AsBool();
		}
		if (Val->Type == EJson::Number)
		{
			return Val->AsNumber() != 0.0;
		}
		if (Val->Type == EJson::String)
		{
			return Val->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase)
				|| Val->AsString().Equals(TEXT("1"));
		}
		return bDefault;
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

	TSharedPtr<FJsonObject> JsonObj(const TSharedPtr<FJsonObject>& Root, const TCHAR* Key)
	{
		return Root.IsValid() && Root->HasField(Key) ? Root->GetObjectField(Key) : nullptr;
	}

	FDLLookCommand ParseLook(const TSharedPtr<FJsonObject>& LookObj)
	{
		if (!LookObj.IsValid())
		{
			return FDLLookCommand();
		}
		const bool bYawAbs = LookObj->HasField(TEXT("yawAbs"));
		const bool bPitchAbs = LookObj->HasField(TEXT("pitchAbs"));
		if (bYawAbs || bPitchAbs)
		{
			return FDLLookCommand::MakeAbsolute(
				bYawAbs, JsonNum(LookObj, TEXT("yawAbs")),
				bPitchAbs, JsonNum(LookObj, TEXT("pitchAbs")));
		}
		return FDLLookCommand::MakeDelta(JsonNum(LookObj, TEXT("yaw")), JsonNum(LookObj, TEXT("pitch")));
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

	FString JsonToString(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}
}

void UDLAgentBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UDLTickSubsystem::StaticClass());
	Super::Initialize(Collection);
#if UE_BUILD_SHIPPING
	bAllowAgentInput = false;
#else
	bAllowAgentInput = true;
	GConfig->GetBool(TEXT("/Script/DestinyLike.DLAgentSettings"), TEXT("bAllowAgentInput"), bAllowAgentInput, GGameIni);
	int32 PortIni = static_cast<int32>(Port);
	GConfig->GetInt(TEXT("/Script/DestinyLike.DLAgentSettings"), TEXT("AgentHttpPort"), PortIni, GGameIni);
	Port = static_cast<uint32>(FMath::Clamp(PortIni, 1024, 65535));
	if (bAllowAgentInput)
	{
		StartListener();
		BindTickClock();
	}
#endif
}

void UDLAgentBridgeSubsystem::Deinitialize()
{
	UnbindTickClock();
	StopListener();
	Super::Deinitialize();
}

void UDLAgentBridgeSubsystem::BindTickClock()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLTickSubsystem* Tick = GI->GetSubsystem<UDLTickSubsystem>())
		{
			FixedTickHandle = Tick->OnFixedGameTick().AddUObject(this, &UDLAgentBridgeSubsystem::TickAgent);
		}
	}
}

void UDLAgentBridgeSubsystem::UnbindTickClock()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDLTickSubsystem* Tick = GI->GetSubsystem<UDLTickSubsystem>())
		{
			Tick->OnFixedGameTick().Remove(FixedTickHandle);
		}
	}
	FixedTickHandle = FDelegateHandle();
}

void UDLAgentBridgeSubsystem::StartListener()
{
	if (!FHttpServerModule::IsAvailable())
	{
		FModuleManager::LoadModuleChecked<FHttpServerModule>(TEXT("HTTPServer"));
	}
	FHttpServerModule& Http = FHttpServerModule::Get();
	Router = Http.GetHttpRouter(Port, true);
	if (!Router.IsValid())
	{
		UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: agent HTTP failed to bind 127.0.0.1:%u"), Port);
		return;
	}

	StateRoute = Router->BindRoute(FHttpPath(TEXT("/state")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleState));
	IntentRoute = Router->BindRoute(FHttpPath(TEXT("/intent")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleIntent));
	SequenceRoute = Router->BindRoute(FHttpPath(TEXT("/sequence")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleSequence));
	GotoRoute = Router->BindRoute(FHttpPath(TEXT("/goto")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleGoto));
	RespawnRoute = Router->BindRoute(FHttpPath(TEXT("/respawn")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleRespawn));
	DirectorRoute = Router->BindRoute(FHttpPath(TEXT("/director")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleDirector));
	HubRoute = Router->BindRoute(FHttpPath(TEXT("/hub")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateUObject(this, &UDLAgentBridgeSubsystem::HandleHub));
	Http.StartAllListeners();
	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike: agent HTTP on 127.0.0.1:%u (GET /state, POST /intent /sequence /goto /respawn /director /hub)"), Port);
}

void UDLAgentBridgeSubsystem::StopListener()
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

bool UDLAgentBridgeSubsystem::IsLoopback(const FHttpServerRequest& Request) const
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

UWorld* UDLAgentBridgeSubsystem::GetWorldSafe() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

ADLPlayerCharacter* UDLAgentBridgeSubsystem::FindLocalPawn() const
{
	return ResolvePawn();
}

ADLPlayerCharacter* UDLAgentBridgeSubsystem::ResolvePawnForSeat(const FGuid& SeatId) const
{
	if (!SeatId.IsValid())
	{
		return nullptr;
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			return Cast<ADLPlayerCharacter>(Lobby->GetDrivenPawn(SeatId));
		}
	}
	return nullptr;
}

ADLPlayerCharacter* UDLAgentBridgeSubsystem::ResolvePawn() const
{
	if (ADLPlayerCharacter* SeatPawn = ResolvePawnForSeat(AgentSeatId))
	{
		return SeatPawn;
	}
	UWorld* World = GetWorldSafe();
	if (!World)
	{
		return nullptr;
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (ADLPlayerCharacter* Human = Lobby->FindHumanPawn())
			{
				return Human;
			}
		}
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

APlayerController* UDLAgentBridgeSubsystem::FindLocalController() const
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

TSharedRef<FJsonObject> UDLAgentBridgeSubsystem::BuildStateJson(const FGuid& SeatId) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	const FGuid ProbeSeat = SeatId.IsValid() ? SeatId : AgentSeatId;
	ADLPlayerCharacter* Char = SeatId.IsValid() ? ResolvePawnForSeat(SeatId) : FindLocalPawn();
	const UDLRemoteAgentPlaybook* Remote = nullptr;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (const UDLParticipantSeat* Seat = Lobby->FindSeat(ProbeSeat))
			{
				Remote = Cast<UDLRemoteAgentPlaybook>(Seat->GetPlaybook());
			}
		}
	}
	if (Char)
	{
		const FVector Loc = Char->GetActorLocation();
		FRotator Control = FRotator::ZeroRotator;
		if (AController* Ctrl = Char->GetController())
		{
			Control = Ctrl->GetControlRotation();
		}
		Root->SetBoolField(TEXT("ok"), true);
		Root->SetNumberField(TEXT("x"), Loc.X);
		Root->SetNumberField(TEXT("y"), Loc.Y);
		Root->SetNumberField(TEXT("z"), Loc.Z);
		Root->SetNumberField(TEXT("yaw"), Control.Yaw);
		Root->SetNumberField(TEXT("pitch"), FRotator::NormalizeAxis(Control.Pitch));
		Root->SetNumberField(TEXT("vz"), Char->GetVelocity().Z);
		Root->SetBoolField(TEXT("alive"), Char->IsCombatAlive());
		DLAgentNavProbe::FillStateJson(Root, GetWorldSafe(), Char);
		if (const UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Root->SetBoolField(TEXT("sliding"), Move->IsSliding());
			Root->SetBoolField(TEXT("dodging"), Move->IsDodging());
			Root->SetBoolField(TEXT("dashing"), Move->IsDashing());
			Root->SetBoolField(TEXT("air"), !Move->IsMovingOnGround());
			Root->SetBoolField(TEXT("diving"), Move->IsDiveReported());
			Root->SetNumberField(TEXT("jumpsLeft"), Move->GetJumpsRemaining());
			Root->SetNumberField(TEXT("crouchAlpha"), Move->GetCrouchAlpha());
			Root->SetNumberField(TEXT("slideDuration"), Move->GetSlideDuration());
			Root->SetNumberField(TEXT("slideDistanceCm"), Move->EstimateSlideTravelCm());
			Root->SetNumberField(TEXT("dashDuration"), Move->GetDashDuration());
			Root->SetNumberField(TEXT("dashDistanceCm"), Move->GetDashDistance());
			Root->SetNumberField(TEXT("dodgeDuration"), Move->GetDodgeDuration());
		}
		if (const UDLDamageableComponent* Dmg = Char->GetDamageable())
		{
			Root->SetNumberField(TEXT("health"), Dmg->GetHealth());
			Root->SetNumberField(TEXT("shield"), Dmg->GetShield());
		}
		else if (const UDLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			Root->SetNumberField(TEXT("health"), HS->GetHealth());
			Root->SetNumberField(TEXT("shield"), HS->GetShield());
		}
		if (const UDLWeaponMotorComponent* Gun = Char->GetWeaponMotor())
		{
			Root->SetStringField(TEXT("gun"), Gun->GetActiveItem().DisplayName);
			Root->SetStringField(TEXT("slot"), Gun->IsSpecialEquipped() ? TEXT("special") : TEXT("primary"));
			Root->SetNumberField(TEXT("ammo"), Gun->GetAmmoInMag());
			Root->SetNumberField(TEXT("reserve"), Gun->GetSpecialReserve());
			Root->SetNumberField(TEXT("adsAlpha"), Gun->GetAdsAlpha());
			Root->SetNumberField(TEXT("recoilPitch"), Gun->GetAdsRecoilPunch().Y);
			Root->SetNumberField(TEXT("recoilYaw"), Gun->GetAdsRecoilPunch().X);
		}
		FDLSightedTarget Sighted;
		if (DLHitscanService::QuerySightedFromPawn(Char, Sighted))
		{
			Root->SetNumberField(TEXT("sightedHealth"), Sighted.Health);
			Root->SetNumberField(TEXT("sightedShield"), Sighted.Shield);
		}
		TArray<FDLRadarContact> Radar;
		DLHitscanService::QueryRadarContacts(Char, Radar);
		Root->SetNumberField(TEXT("radarBlips"), Radar.Num());
		Root->SetNumberField(TEXT("radarRipple"), DLHitscanService::QueryRadarRippleMask(Char));
		if (const UWorld* World = GetWorldSafe())
		{
			if (const ADLGameStateBase* GS = World->GetGameState<ADLGameStateBase>())
			{
				Root->SetNumberField(TEXT("teamAScore"), GS->GetTeamAScore());
				Root->SetNumberField(TEXT("teamBScore"), GS->GetTeamBScore());
				Root->SetStringField(TEXT("scoreLine"), GS->GetScoreLine());
			}
		}
	}
	else
	{
		Root->SetBoolField(TEXT("ok"), false);
		Root->SetStringField(TEXT("error"), TEXT("no_local_pawn"));
	}

	if (const APlayerController* PC = FindLocalController())
	{
		if (const ADLPlayerCharacter* Viewed = Cast<ADLPlayerCharacter>(PC->GetViewTarget()))
		{
			if (const UDLDamageableComponent* Dmg = Viewed->GetDamageable())
			{
				Root->SetNumberField(TEXT("viewHealth"), Dmg->GetHealth());
				Root->SetNumberField(TEXT("viewShield"), Dmg->GetShield());
			}
			else if (const UDLHealthShieldComponent* HS = Viewed->GetHealthShield())
			{
				Root->SetNumberField(TEXT("viewHealth"), HS->GetHealth());
				Root->SetNumberField(TEXT("viewShield"), HS->GetShield());
			}
			TArray<FDLRadarContact> ViewRadar;
			DLHitscanService::QueryRadarContacts(Viewed, ViewRadar);
			Root->SetNumberField(TEXT("viewRadarBlips"), ViewRadar.Num());
		}
	}

	if (Remote)
	{
		Root->SetNumberField(TEXT("seqRemaining"), Remote->RemainingSeconds());
		Root->SetBoolField(TEXT("goto"), Remote->IsGotoActive());
		if (Remote->IsGotoActive())
		{
			const FVector Goal = Remote->GetGotoGoal();
			Root->SetNumberField(TEXT("gotoX"), Goal.X);
			Root->SetNumberField(TEXT("gotoY"), Goal.Y);
			Root->SetNumberField(TEXT("gotoZ"), Goal.Z);
		}
	}
	else
	{
		Root->SetNumberField(TEXT("seqRemaining"), SequenceRunner.RemainingSeconds());
		Root->SetNumberField(TEXT("seqStep"), SequenceRunner.IsActive() ? SequenceRunner.Index : -1);
		Root->SetNumberField(TEXT("seqCount"), SequenceRunner.Steps.Num());
		Root->SetBoolField(TEXT("goto"), GotoDriver.bActive);
		if (GotoDriver.bActive)
		{
			Root->SetNumberField(TEXT("gotoX"), GotoDriver.Goal.X);
			Root->SetNumberField(TEXT("gotoY"), GotoDriver.Goal.Y);
			Root->SetNumberField(TEXT("gotoZ"), GotoDriver.Goal.Z);
		}
	}
	FillSceneMenu(Root);
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->FillStateJson(Root);
		}
		if (const UDLSessionHub* Hub = GI->GetSubsystem<UDLSessionHub>())
		{
			Root->SetNumberField(TEXT("hubPort"), Hub->GetPort());
			Root->SetBoolField(TEXT("hub"), Hub->IsListening());
		}
	}
	if (AgentSeatId.IsValid())
	{
		Root->SetStringField(TEXT("agentSeat"), AgentSeatId.ToString(EGuidFormats::DigitsWithHyphens));
	}
	if (ProbeSeat.IsValid())
	{
		Root->SetStringField(TEXT("probeSeat"), ProbeSeat.ToString(EGuidFormats::DigitsWithHyphens));
	}
	return Root;
}

FString UDLAgentBridgeSubsystem::BuildStateJsonString(const FGuid& SeatId) const
{
	return JsonToString(BuildStateJson(SeatId));
}

bool UDLAgentBridgeSubsystem::HandleState(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
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
	OnComplete(FHttpServerResponse::Create(BuildStateJsonString(SeatQuery), TEXT("application/json")));
	return true;
}

void UDLAgentBridgeSubsystem::CancelSequenceAndGoto(bool bClearPawn)
{
	SequenceRunner.Cancel();
	GotoDriver.Cancel();
	if (bClearPawn)
	{
		if (ADLPlayerCharacter* Char = FindLocalPawn())
		{
			Char->ClearAgentIntent();
		}
	}
}

void UDLAgentBridgeSubsystem::ApplyIntentObject(const TSharedPtr<FJsonObject>& Root, bool bCancelQueue)
{
	if (bCancelQueue)
	{
		CancelSequenceAndGoto(false);
	}

	ADLPlayerCharacter* Char = FindLocalPawn();
	if (!Char)
	{
		return;
	}

	FVector2D Move = FVector2D::ZeroVector;
	FDLLookCommand LookCmd;
	if (const TSharedPtr<FJsonObject> MoveObj = JsonObj(Root, TEXT("move")))
	{
		Move.X = JsonNum(MoveObj, TEXT("x"));
		Move.Y = JsonNum(MoveObj, TEXT("y"));
	}
	FString TrackSeat;
	FGuid TrackSeatId;
	if (Root->TryGetStringField(TEXT("lookAtSeat"), TrackSeat) && FGuid::Parse(TrackSeat, TrackSeatId) && TrackSeatId.IsValid())
	{
		Char->SetLookTrackSeat(TrackSeatId);
	}
	else if (const TSharedPtr<FJsonObject> LookObj = JsonObj(Root, TEXT("look")))
	{
		LookCmd = ParseLook(LookObj);
		ApplyLookCommand(Char, LookCmd);
	}

	FDLAgentIntent Intent;
	Intent.Move = Move;
	Intent.Look = LookCmd.GetDelta();
	Intent.bSprint = JsonBool(Root, TEXT("sprint"));
	Intent.bCrouch = JsonBool(Root, TEXT("crouch"));
	Intent.bADS = JsonBool(Root, TEXT("ads"));
	Intent.bFire = JsonBool(Root, TEXT("fire"));
	Intent.bJump = JsonBool(Root, TEXT("jump"));
	Intent.bDodge = JsonBool(Root, TEXT("dodge"));
	Intent.bDash = JsonBool(Root, TEXT("dash"));
	Intent.bReload = JsonBool(Root, TEXT("reload"));
	Intent.bSwap = JsonBool(Root, TEXT("swap"));
	Intent.bSlide = JsonBool(Root, TEXT("slide"));
	Intent.bAirDive = JsonBool(Root, TEXT("airDive"));
	Intent.bMelee = JsonBool(Root, TEXT("melee"));
	FString Weapon;
	if (Root->TryGetStringField(TEXT("weapon"), Weapon))
	{
		if (Weapon.Equals(TEXT("primary"), ESearchCase::IgnoreCase))
		{
			Intent.bWeaponPrimary = true;
		}
		else if (Weapon.Equals(TEXT("special"), ESearchCase::IgnoreCase) || Weapon.Equals(TEXT("secondary"), ESearchCase::IgnoreCase))
		{
			Intent.bWeaponSpecial = true;
		}
	}
	FString Sight;
	if (Root->TryGetStringField(TEXT("sight"), Sight))
	{
		Intent.SightId = FName(*Sight);
	}
	Char->ApplyAgentIntent(Intent);
}

FDLAgentStep UDLAgentBridgeSubsystem::ParseStep(const TSharedPtr<FJsonObject>& Obj) const
{
	FDLAgentStep Step;
	if (!Obj.IsValid())
	{
		return Step;
	}
	Step.Seconds = FMath::Max(0.f, JsonNum(Obj, TEXT("seconds")));
	if (const TSharedPtr<FJsonObject> MoveObj = JsonObj(Obj, TEXT("move")))
	{
		Step.Move.X = JsonNum(MoveObj, TEXT("x"));
		Step.Move.Y = JsonNum(MoveObj, TEXT("y"));
	}
	if (const TSharedPtr<FJsonObject> LookObj = JsonObj(Obj, TEXT("look")))
	{
		Step.Look = ParseLook(LookObj);
	}
	FString TrackSeat;
	if (Obj->TryGetStringField(TEXT("lookAtSeat"), TrackSeat))
	{
		FGuid::Parse(TrackSeat, Step.TrackSeatId);
	}
	Step.bSprint = JsonBool(Obj, TEXT("sprint"));
	Step.bCrouch = JsonBool(Obj, TEXT("crouch"));
	Step.bADS = JsonBool(Obj, TEXT("ads"));
	Step.bFire = JsonBool(Obj, TEXT("fire"));
	Step.bJump = JsonBool(Obj, TEXT("jump"));
	Step.bDodge = JsonBool(Obj, TEXT("dodge"));
	Step.bDash = JsonBool(Obj, TEXT("dash"));
	Step.bReload = JsonBool(Obj, TEXT("reload"));
	Step.bSwap = JsonBool(Obj, TEXT("swap"));
	Step.bSlide = JsonBool(Obj, TEXT("slide"));
	Step.bAirDive = JsonBool(Obj, TEXT("airDive"));
	Step.bMelee = JsonBool(Obj, TEXT("melee"));
	FString Weapon;
	if (Obj->TryGetStringField(TEXT("weapon"), Weapon))
	{
		if (Weapon.Equals(TEXT("primary"), ESearchCase::IgnoreCase))
		{
			Step.bWeaponPrimary = true;
		}
		else if (Weapon.Equals(TEXT("special"), ESearchCase::IgnoreCase) || Weapon.Equals(TEXT("secondary"), ESearchCase::IgnoreCase))
		{
			Step.bWeaponSpecial = true;
		}
	}
	FString Sight;
	if (Obj->TryGetStringField(TEXT("sight"), Sight))
	{
		Step.SightId = FName(*Sight);
	}
	return Step;
}

bool UDLAgentBridgeSubsystem::ParseSteps(const TSharedPtr<FJsonObject>& Root, TArray<FDLAgentStep>& OutSteps, bool& bAfterCurrent) const
{
	OutSteps.Reset();
	bAfterCurrent = false;
	if (!Root.IsValid())
	{
		return true;
	}

	FString ReplaceFrom;
	if (Root->TryGetStringField(TEXT("replaceFrom"), ReplaceFrom))
	{
		bAfterCurrent = ReplaceFrom.Equals(TEXT("afterCurrent"), ESearchCase::IgnoreCase)
			|| ReplaceFrom.Equals(TEXT("remainder"), ESearchCase::IgnoreCase);
	}

	const TArray<TSharedPtr<FJsonValue>>* StepsJson = nullptr;
	if (!Root->TryGetArrayField(TEXT("steps"), StepsJson) || !StepsJson)
	{
		return true;
	}

	for (const TSharedPtr<FJsonValue>& Val : *StepsJson)
	{
		if (!Val.IsValid() || Val->Type != EJson::Object)
		{
			continue;
		}
		OutSteps.Add(ParseStep(Val->AsObject()));
	}
	return true;
}

bool UDLAgentBridgeSubsystem::QueueSteps(const TArray<FDLAgentStep>& Steps, bool bAfterCurrent, FString& OutError)
{
	if (!FindLocalPawn())
	{
		OutError = TEXT("no_local_pawn");
		return false;
	}
	GotoDriver.Cancel();
	return SequenceRunner.Queue(Steps, bAfterCurrent, OutError);
}

void UDLAgentBridgeSubsystem::ApplyLookCommand(ADLPlayerCharacter* Char, const FDLLookCommand& Look) const
{
	if (!Char)
	{
		return;
	}
	Char->ApplyAgentLookCommand(Look);
}

void UDLAgentBridgeSubsystem::ApplyStepPulses(ADLPlayerCharacter* Char, const FDLAgentStep& Step)
{
	if (!Char)
	{
		return;
	}
	Char->ApplyAgentLookFromStep(Step.TrackSeatId, Step.Look);
	Char->ApplyAgentIntent(Step.ToIntent(true));
}

void UDLAgentBridgeSubsystem::ApplyStepHolds(ADLPlayerCharacter* Char, const FDLAgentStep& Step)
{
	if (!Char)
	{
		return;
	}
	Char->ApplyAgentLookFromStep(Step.TrackSeatId, Step.Look);
	Char->ApplyAgentIntent(Step.ToIntent(false));
}

bool UDLAgentBridgeSubsystem::StartGoto(const FVector& Dest, FString& OutError, bool bFromRepath)
{
	ADLPlayerCharacter* Char = FindLocalPawn();
	UWorld* World = GetWorldSafe();
	if (!bFromRepath)
	{
		SequenceRunner.Cancel();
	}
	return GotoDriver.Start(World, Char, Dest, OutError, bFromRepath);
}

void UDLAgentBridgeSubsystem::TickAgent(float DeltaSeconds)
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (!Lobby->IsGameplayUnlocked())
			{
				return;
			}
		}
	}
	ADLPlayerCharacter* Char = FindLocalPawn();
	if (!Char)
	{
		if (SequenceRunner.IsActive() || GotoDriver.bActive)
		{
			CancelSequenceAndGoto(false);
		}
		return;
	}

	if (GotoDriver.bActive)
	{
		GotoDriver.Tick(DeltaSeconds, GetWorldSafe(), Char);
	}
	else if (SequenceRunner.IsActive())
	{
		SequenceRunner.Tick(DeltaSeconds, Char,
			[this](ADLPlayerCharacter* C, const FDLAgentStep& S) { ApplyStepPulses(C, S); },
			[this](ADLPlayerCharacter* C, const FDLAgentStep& S) { ApplyStepHolds(C, S); });
	}
}

bool UDLAgentBridgeSubsystem::HandleIntent(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	if (!FindLocalPawn())
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_local_pawn")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}

	ApplyIntentObject(Root, true);
	OnComplete(FHttpServerResponse::Create(TEXT("{\"ok\":true}"), TEXT("application/json")));
	return true;
}

bool UDLAgentBridgeSubsystem::HandleSequence(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
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

	TArray<FDLAgentStep> Steps;
	bool bAfterCurrent = false;
	ParseSteps(Root, Steps, bAfterCurrent);
	if (Steps.Num() == 0 && !bAfterCurrent)
	{
		CancelSequenceAndGoto(true);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), true);
		Out->SetNumberField(TEXT("queued"), 0);
		Out->SetNumberField(TEXT("seconds"), 0);
		OnComplete(FHttpServerResponse::Create(JsonToString(Out), TEXT("application/json")));
		return true;
	}
	FString Error;
	if (!QueueSteps(Steps, bAfterCurrent, Error))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, *Error));
		return true;
	}

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("ok"), true);
	Out->SetNumberField(TEXT("queued"), SequenceRunner.Steps.Num());
	float Remaining = 0.f;
	for (const FDLAgentStep& Step : SequenceRunner.Steps)
	{
		Remaining += Step.Seconds;
	}
	Out->SetNumberField(TEXT("seconds"), Remaining);
	OnComplete(FHttpServerResponse::Create(JsonToString(Out), TEXT("application/json")));
	return true;
}

bool UDLAgentBridgeSubsystem::HandleGoto(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
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
	const FVector Dest(JsonNum(Root, TEXT("x")), JsonNum(Root, TEXT("y")), JsonNum(Root, TEXT("z")));
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLLobbySubsystem* Lobby = GI->GetSubsystem<UDLLobbySubsystem>())
		{
			if (!Lobby->IsGameplayUnlocked())
			{
				OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("lobby_locked")));
				return true;
			}
		}
	}
	FString Error;
	if (!StartGoto(Dest, Error))
	{
		const EHttpServerResponseCodes Code = Error == TEXT("no_local_pawn")
			? EHttpServerResponseCodes::NotFound
			: EHttpServerResponseCodes::BadRequest;
		OnComplete(FHttpServerResponse::Error(Code, *Error));
		return true;
	}

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("ok"), true);
	Out->SetNumberField(TEXT("x"), GotoDriver.Goal.X);
	Out->SetNumberField(TEXT("y"), GotoDriver.Goal.Y);
	Out->SetNumberField(TEXT("z"), GotoDriver.Goal.Z);
	Out->SetNumberField(TEXT("waypoints"), GotoDriver.Path.Num());
	Out->SetBoolField(TEXT("partial"), GotoDriver.bPartial);
	OnComplete(FHttpServerResponse::Create(JsonToString(Out), TEXT("application/json")));
	return true;
}

bool UDLAgentBridgeSubsystem::HandleRespawn(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	CancelSequenceAndGoto(true);

	APlayerController* PC = FindLocalController();
	UWorld* World = GetWorldSafe();
	ADLGameModeBase* GM = World ? World->GetAuthGameMode<ADLGameModeBase>() : nullptr;
	if (!PC || !GM)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_local_controller")));
		return true;
	}

	GM->RequestRespawn(PC);
	OnComplete(FHttpServerResponse::Create(BuildStateJsonString(), TEXT("application/json")));
	return true;
}

void UDLAgentBridgeSubsystem::FillSceneMenu(TSharedRef<FJsonObject> Root) const
{
	EDLSceneId Scene = EDLSceneId::Boot;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UDLSceneRouter* Scenes = GI->GetSubsystem<UDLSceneRouter>())
		{
			Scene = Scenes->GetCurrentScene();
		}
	}
	if (UWorld* World = GetWorldSafe())
	{
		if (const ADLGameModeBase* GM = World->GetAuthGameMode<ADLGameModeBase>())
		{
			Scene = GM->GetSceneId();
		}
	}
	Root->SetStringField(TEXT("scene"), SceneName(Scene));
	bool bMenu = false;
	if (const ADLPlayerController* PC = Cast<ADLPlayerController>(FindLocalController()))
	{
		if (const UDLMainMenuOverlay* Menu = PC->GetMainMenu())
		{
			bMenu = Menu->IsOverlayVisible();
		}
	}
	Root->SetBoolField(TEXT("menu"), bMenu);
}

bool UDLAgentBridgeSubsystem::HandleDirector(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	ADLPlayerController* PC = Cast<ADLPlayerController>(FindLocalController());
	if (!PC)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_local_controller")));
		return true;
	}

	UDLMainMenuOverlay* Menu = PC->GetMainMenu();
	if (!Menu)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::NotFound, TEXT("no_menu")));
		return true;
	}

	TSharedPtr<FJsonObject> Root;
	FString ParseError;
	if (!ParseJson(BodyToString(Request.Body), Root, ParseError))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("invalid_json")));
		return true;
	}
	FString Action;
	if (Root.IsValid())
	{
		Root->TryGetStringField(TEXT("action"), Action);
	}
	Action = Action.ToLower();
	if (Action.IsEmpty())
	{
		Action = TEXT("toggle");
	}

	if (Action == TEXT("open"))
	{
		PC->SetMainMenuOpen(true);
	}
	else if (Action == TEXT("close"))
	{
		PC->SetMainMenuOpen(false);
	}
	else if (Action == TEXT("toggle"))
	{
		PC->ToggleMainMenu();
	}
	else if (Action == TEXT("director") || Action == TEXT("directortab"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
	}
	else if (Action == TEXT("keybinds") || Action == TEXT("keybindstab"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowKeybindsTab();
	}
	else if (Action == TEXT("pvp") || Action == TEXT("composer"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		DLActivityLauncher::Travel(PC, EDLSceneId::Composer);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("arena"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		DLActivityLauncher::Travel(PC, EDLSceneId::Pvp);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("raid"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		DLActivityLauncher::Travel(PC, EDLSceneId::Raid);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("practice"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		DLActivityLauncher::Travel(PC, EDLSceneId::Practice);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("social"))
	{
		PC->SetMainMenuOpen(true);
		Menu->ShowDirectorTab();
		DLActivityLauncher::ExitToSocial(PC);
		Menu->HideOverlay();
	}
	else if (Action == TEXT("ready"))
	{
		if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->ToggleLocalReady();
		}
	}
	else if (Action == TEXT("host"))
	{
		if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->ClaimLocalHost();
		}
	}
	else if (Action == TEXT("guest"))
	{
		if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->ClaimLocalGuest();
		}
	}
	else if (Action == TEXT("go") || Action == TEXT("start"))
	{
		if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
		{
			Lobby->RequestLocalGo();
		}
	}
	else if (Action == TEXT("join"))
	{
		if (UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>())
		{
			FString Error;
			if (UDLParticipantSeat* Seat = Lobby->JoinRemoteAgent(TEXT("cursor"), false, Error, TEXT("cursor")))
			{
				AgentSeatId = Seat->GetSeatId();
			}
		}
	}
	else
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("unknown_action")));
		return true;
	}

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("ok"), true);
	Out->SetStringField(TEXT("action"), Action);
	FillSceneMenu(Out);
	OnComplete(FHttpServerResponse::Create(JsonToString(Out), TEXT("application/json")));
	return true;
}

bool UDLAgentBridgeSubsystem::HandleHub(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsLoopback(Request))
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("loopback_only")));
		return true;
	}

	UDLLobbySubsystem* Lobby = GetGameInstance()->GetSubsystem<UDLLobbySubsystem>();
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

	FString Type = JsonStr(Root, TEXT("type")).ToLower();
	if (Type == TEXT("plan"))
	{
		TArray<FDLAgentStep> Steps;
		bool bRemainder = false;
		ParseSteps(Root, Steps, bRemainder);
		FGuid SeatId;
		FGuid::Parse(JsonStr(Root, TEXT("seatId")), SeatId);
		if (!SeatId.IsValid())
		{
			SeatId = AgentSeatId.IsValid() ? AgentSeatId : Lobby->GetLastJoinedSeatId();
		}
		FString Error;
		const bool bOk = Lobby->QueuePlan(SeatId, Steps, bRemainder, Error);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), bOk);
		if (!bOk)
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		OnComplete(FHttpServerResponse::Create(JsonToString(Out), TEXT("application/json")));
		return true;
	}

	const TSharedRef<FJsonObject> Out = Lobby->HandleMessage(Root);
	if (Type == TEXT("join") && Out->GetBoolField(TEXT("ok")))
	{
		FString SeatStr;
		if (Out->TryGetStringField(TEXT("seatId"), SeatStr))
		{
			FGuid::Parse(SeatStr, AgentSeatId);
		}
	}
	OnComplete(FHttpServerResponse::Create(JsonToString(Out), TEXT("application/json")));
	return true;
}
