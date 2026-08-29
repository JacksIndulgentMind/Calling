#include "Game/CLHubDriveTrace.h"
#include "Game/CLAgentCodec.h"
#include "Game/CLInstanceIdentity.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLLoopbackJoin.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLSeatMotor.h"
#include "AI/CLSeatController.h"
#include "Player/CLCombatPawn.h"
#include "Player/CLPlayerController.h"
#include "Core/CLLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

using namespace CLAgentCodec;

namespace
{
	FCLHubDriveSnap GLast;

	FString NetModeName(const UWorld* World)
	{
		if (!World)
		{
			return TEXT("none");
		}
		switch (World->GetNetMode())
		{
		case NM_Client: return TEXT("client");
		case NM_ListenServer: return TEXT("listen");
		case NM_DedicatedServer: return TEXT("dedicated");
		default: return TEXT("standalone");
		}
	}

	FGuid ResolveSeatId(UCLLobbySubsystem* Lobby, const TSharedPtr<FJsonObject>& Root, const FGuid* FallbackSeat)
	{
		FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		if (!SeatId.IsValid() && FallbackSeat && FallbackSeat->IsValid())
		{
			SeatId = *FallbackSeat;
		}
		if (!SeatId.IsValid() && Lobby)
		{
			SeatId = Lobby->GetLastJoinedSeatId();
		}
		if (SeatId.IsValid() && Lobby && !Lobby->FindSeat(SeatId))
		{
			if (UWorld* World = Lobby->GetWorld())
			{
				if (World->GetNetMode() == NM_Client)
				{
					SeatId = Lobby->GetLastJoinedSeatId();
				}
			}
		}
		return SeatId;
	}

	APlayerController* BoundControllerForPawn(UCLLobbySubsystem* Lobby, const APawn* Pawn)
	{
		if (!Lobby || !Pawn)
		{
			return nullptr;
		}
		for (UCLParticipantSeat* Seat : Lobby->GetSeats())
		{
			if (!Seat)
			{
				continue;
			}
			if (APlayerController* PC = Seat->GetBoundController())
			{
				if (PC->GetPawn() == Pawn)
				{
					return PC;
				}
			}
		}
		return nullptr;
	}

	FString ClassifyDriven(UCLLobbySubsystem* Lobby, APawn* Pawn, UCLParticipantSeat* Seat)
	{
		if (!Pawn)
		{
			return TEXT("none");
		}
		if (Pawn->IsA<ACLCombatPawn>())
		{
			return TEXT("npc");
		}
		APlayerController* Bound = Seat ? Seat->GetBoundController() : nullptr;
		if (!Bound)
		{
			Bound = BoundControllerForPawn(Lobby, Pawn);
		}
		if (Bound)
		{
			return Bound->IsLocalController() ? TEXT("localHuman") : TEXT("remoteHuman");
		}
		if (Seat && Seat->IsHeadlessJoin())
		{
			if (Pawn->IsLocallyControlled())
			{
				return TEXT("localHuman");
			}
			if (Cast<ACLSeatController>(Pawn->GetController()))
			{
				return TEXT("headlessBot");
			}
		}
		if (Pawn->IsLocallyControlled())
		{
			return TEXT("localHuman");
		}
		if (Cast<ACLSeatController>(Pawn->GetController()))
		{
			return TEXT("headlessBot");
		}
		return TEXT("remoteHuman");
	}

	bool IsDriveType(const FString& Type)
	{
		return Type == TEXT("appendbotbook") || Type == TEXT("branchbotbook") || Type == TEXT("mindcontrol")
			|| Type == TEXT("plan") || Type == TEXT("goto") || Type == TEXT("clearbotbook");
	}
}

void CLHubDriveTrace::StampListen(const TSharedPtr<FJsonObject>& Root, int32 ListenPort, const TCHAR* Recv)
{
	if (!Root.IsValid())
	{
		return;
	}
	Root->SetNumberField(TEXT("listenPort"), ListenPort);
	if (JsonStr(Root, TEXT("recv")).IsEmpty())
	{
		Root->SetStringField(TEXT("recv"), Recv);
	}
}

void CLHubDriveTrace::StampViaRpc(const TSharedPtr<FJsonObject>& Root, int32 FromListenPort)
{
	if (!Root.IsValid())
	{
		return;
	}
	const FString PrevRecv = JsonStr(Root, TEXT("recv"));
	if (!PrevRecv.IsEmpty())
	{
		Root->SetStringField(TEXT("fromRecv"), PrevRecv);
	}
	Root->SetStringField(TEXT("recv"), TEXT("viaRpc"));
	Root->SetNumberField(TEXT("fromPort"), FromListenPort);
	if (JsonNum(Root, TEXT("listenPort")) <= 0.f)
	{
		Root->SetNumberField(TEXT("listenPort"), FromListenPort);
	}
}

void CLHubDriveTrace::NotePlayerController(ACLPlayerController* PC, const TSharedPtr<FJsonObject>& Root)
{
	if (!PC)
	{
		return;
	}
	const int32 ListenPort = static_cast<int32>(JsonNum(Root, TEXT("listenPort")));
	const FString Recv = JsonStr(Root, TEXT("recv"));
	FString Intended = JsonStr(Root, TEXT("intendedTarget"));
	if (Intended.IsEmpty())
	{
		Intended = JsonStr(Root, TEXT("intend"));
	}
	PC->NoteHubReceive(ListenPort, Recv, Intended);
}

void CLHubDriveTrace::FillAndLog(UCLLobbySubsystem* Lobby, const TSharedPtr<FJsonObject>& Root, const FGuid* FallbackSeat,
	ACLPlayerController* RecvPC)
{
	GLast = FCLHubDriveSnap();
	if (!Lobby || !Root.IsValid())
	{
		return;
	}

	GLast.ListenPort = static_cast<int32>(JsonNum(Root, TEXT("listenPort")));
	GLast.FromPort = static_cast<int32>(JsonNum(Root, TEXT("fromPort")));
	GLast.ExecPort = CLLoopbackJoin::AgentHttpPort();
	GLast.Recv = JsonStr(Root, TEXT("recv"));
	GLast.FromRecv = JsonStr(Root, TEXT("fromRecv"));
	GLast.Type = JsonStr(Root, TEXT("type")).ToLower();
	GLast.IntendedTarget = JsonStr(Root, TEXT("intendedTarget"));
	if (GLast.IntendedTarget.IsEmpty())
	{
		GLast.IntendedTarget = JsonStr(Root, TEXT("intend"));
	}
	if (GLast.IntendedTarget.IsEmpty())
	{
		GLast.IntendedTarget = TEXT("unspecified");
	}

	if (UGameInstance* GI = Lobby->GetGameInstance())
	{
		if (UCLInstanceIdentitySubsystem* Id = GI->GetSubsystem<UCLInstanceIdentitySubsystem>())
		{
			GLast.InstanceId = GuidStr(Id->GetInstanceId());
			FGuid Agent = ParseGuid(JsonStr(Root, TEXT("agentId")));
			if (!Agent.IsValid())
			{
				Agent = Id->GetLastAgentId();
			}
			GLast.AgentId = GuidStr(Agent);
			GLast.OriginInstanceId = GuidStr(Id->GetOriginInstanceId());
			if (GLast.OriginInstanceId.IsEmpty())
			{
				const FGuid FromInst = ParseGuid(JsonStr(Root, TEXT("originInstanceId")));
				if (FromInst.IsValid() && FromInst != Id->GetInstanceId())
				{
					GLast.OriginInstanceId = GuidStr(FromInst);
				}
			}
			FGuid Requestor = ParseGuid(JsonStr(Root, TEXT("requestorId")));
			if (!Requestor.IsValid())
			{
				Requestor = Agent;
			}
			GLast.RequestorId = GuidStr(Requestor);
		}
	}

	GLast.bUnlocked = Lobby->IsGameplayUnlocked();
	UWorld* World = Lobby->GetWorld();
	APlayerController* LocalPC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (RecvPC)
	{
		LocalPC = RecvPC;
	}
	GLast.bPcLocal = LocalPC && LocalPC->IsLocalController();
	if (LocalPC)
	{
		GLast.bIgnoreMove = LocalPC->IsMoveInputIgnored();
		GLast.bIgnoreLook = LocalPC->IsLookInputIgnored();
	}

	FGuid SeatId = ResolveSeatId(Lobby, Root, FallbackSeat);
	if (GLast.Type == TEXT("mindcontrol"))
	{
		const FGuid TargetId = ParseGuid(JsonStr(Root, TEXT("targetSeatId")));
		if (TargetId.IsValid())
		{
			SeatId = TargetId;
		}
	}
	GLast.SeatId = SeatId.IsValid() ? GuidStr(SeatId) : FString();

	const FGuid IntendedSeat = ParseGuid(GLast.IntendedTarget);
	if (IntendedSeat.IsValid())
	{
		SeatId = IntendedSeat;
		GLast.SeatId = GuidStr(SeatId);
	}

	UCLParticipantSeat* Seat = SeatId.IsValid() ? Lobby->FindSeat(SeatId) : nullptr;
	if (Seat)
	{
		if (UGameInstance* GI = Lobby->GetGameInstance())
		{
			if (UCLInstanceIdentitySubsystem* Id = GI->GetSubsystem<UCLInstanceIdentitySubsystem>())
			{
				Id->BindSeat(Seat);
				const FGuid Requestor = ParseGuid(JsonStr(Root, TEXT("requestorId")));
				if (Requestor.IsValid())
				{
					Seat->SetRequestorId(Requestor);
				}
			}
		}
	}
	APawn* Pawn = Seat ? Seat->GetDrivenPawn() : nullptr;
	if (!Pawn && LocalPC)
	{
		Pawn = LocalPC->GetPawn();
	}
	if (Pawn)
	{
		GLast.Loc = Pawn->GetActorLocation();
		GLast.bDrivenLocal = Pawn->IsLocallyControlled();
		GLast.bDrivenHasCtrl = Pawn->GetController() != nullptr;
		GLast.Role = static_cast<int32>(Pawn->GetLocalRole());
		GLast.RemoteRole = static_cast<int32>(Pawn->GetRemoteRole());
	}
	GLast.DrivenKind = ClassifyDriven(Lobby, Pawn, Seat);

	if (IsDriveType(GLast.Type) && GLast.DrivenKind == TEXT("remoteHuman"))
	{
		GLast.Alert = TEXT("remote_player_pawn");
	}
	else if (IsDriveType(GLast.Type) && GLast.IntendedTarget == TEXT("localHuman") && GLast.DrivenKind != TEXT("localHuman")
		&& GLast.DrivenKind != TEXT("none"))
	{
		GLast.Alert = TEXT("intended_target_mismatch");
	}
	else if (IsDriveType(GLast.Type) && GLast.IntendedTarget == TEXT("headlessBot") && GLast.DrivenKind == TEXT("remoteHuman"))
	{
		GLast.Alert = TEXT("remote_player_pawn");
	}

	const TCHAR* Level = GLast.Alert.IsEmpty() ? TEXT("HubDrive") : TEXT("HubDrive ALERT");
	if (!GLast.Alert.IsEmpty())
	{
		UE_LOG(LogCallingHub, Error,
			TEXT("%s type=%s recv=%s listenPort=%d fromPort=%d execPort=%d net=%s instance=%s agent=%s requestor=%s origin=%s intended=%s seat=%s driven=%s local=%d hasCtrl=%d role=%d remote=%d ignoreMove=%d ignoreLook=%d unlocked=%d pcLocal=%d loc=(%.0f,%.0f,%.0f) alert=%s"),
			Level, *GLast.Type, *GLast.Recv, GLast.ListenPort, GLast.FromPort, GLast.ExecPort, *NetModeName(World),
			*GLast.InstanceId, *GLast.AgentId, *GLast.RequestorId, *GLast.OriginInstanceId,
			*GLast.IntendedTarget, *GLast.SeatId, *GLast.DrivenKind, GLast.bDrivenLocal ? 1 : 0, GLast.bDrivenHasCtrl ? 1 : 0,
			GLast.Role, GLast.RemoteRole, GLast.bIgnoreMove ? 1 : 0, GLast.bIgnoreLook ? 1 : 0, GLast.bUnlocked ? 1 : 0,
			GLast.bPcLocal ? 1 : 0, GLast.Loc.X, GLast.Loc.Y, GLast.Loc.Z, *GLast.Alert);
	}
	else
	{
		UE_LOG(LogCallingHub, Display,
			TEXT("%s type=%s recv=%s listenPort=%d fromPort=%d execPort=%d net=%s instance=%s agent=%s requestor=%s origin=%s intended=%s seat=%s driven=%s local=%d hasCtrl=%d role=%d remote=%d ignoreMove=%d ignoreLook=%d unlocked=%d pcLocal=%d loc=(%.0f,%.0f,%.0f)"),
			Level, *GLast.Type, *GLast.Recv, GLast.ListenPort, GLast.FromPort, GLast.ExecPort, *NetModeName(World),
			*GLast.InstanceId, *GLast.AgentId, *GLast.RequestorId, *GLast.OriginInstanceId,
			*GLast.IntendedTarget, *GLast.SeatId, *GLast.DrivenKind, GLast.bDrivenLocal ? 1 : 0, GLast.bDrivenHasCtrl ? 1 : 0,
			GLast.Role, GLast.RemoteRole, GLast.bIgnoreMove ? 1 : 0, GLast.bIgnoreLook ? 1 : 0, GLast.bUnlocked ? 1 : 0,
			GLast.bPcLocal ? 1 : 0, GLast.Loc.X, GLast.Loc.Y, GLast.Loc.Z);
	}

	if (RecvPC)
	{
		NotePlayerController(RecvPC, Root);
	}
}

void CLHubDriveTrace::ApplyToJson(const TSharedRef<FJsonObject>& Out)
{
	Out->SetNumberField(TEXT("listenPort"), GLast.ListenPort);
	Out->SetNumberField(TEXT("execPort"), GLast.ExecPort);
	if (GLast.FromPort > 0)
	{
		Out->SetNumberField(TEXT("fromPort"), GLast.FromPort);
	}
	if (!GLast.Recv.IsEmpty())
	{
		Out->SetStringField(TEXT("recv"), GLast.Recv);
	}
	if (!GLast.IntendedTarget.IsEmpty())
	{
		Out->SetStringField(TEXT("intendedTarget"), GLast.IntendedTarget);
	}
	if (!GLast.DrivenKind.IsEmpty())
	{
		Out->SetStringField(TEXT("drivenKind"), GLast.DrivenKind);
	}
	if (!GLast.Alert.IsEmpty())
	{
		Out->SetStringField(TEXT("alert"), GLast.Alert);
	}
	Out->SetBoolField(TEXT("drivenLocal"), GLast.bDrivenLocal);
	Out->SetBoolField(TEXT("ignoreMove"), GLast.bIgnoreMove);
	if (!GLast.InstanceId.IsEmpty())
	{
		Out->SetStringField(TEXT("instanceId"), GLast.InstanceId);
	}
	if (!GLast.AgentId.IsEmpty())
	{
		Out->SetStringField(TEXT("agentId"), GLast.AgentId);
	}
	if (!GLast.OriginInstanceId.IsEmpty())
	{
		Out->SetStringField(TEXT("originInstanceId"), GLast.OriginInstanceId);
	}
	if (!GLast.RequestorId.IsEmpty())
	{
		Out->SetStringField(TEXT("requestorId"), GLast.RequestorId);
	}
}

const FCLHubDriveSnap& CLHubDriveTrace::Last()
{
	return GLast;
}
