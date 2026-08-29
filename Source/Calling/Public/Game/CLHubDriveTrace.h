#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UCLLobbySubsystem;
class UCLParticipantSeat;
class APawn;
class ACLPlayerController;

/** Last hub-drive classify for this game-thread Dispatch. */
struct FCLHubDriveSnap
{
	int32 ListenPort = 0;
	int32 ExecPort = 0;
	int32 FromPort = 0;
	FString Recv;
	FString FromRecv;
	FString Type;
	FString SeatId;
	FString IntendedTarget;
	FString DrivenKind;
	FString Alert;
	FString InstanceId;
	FString AgentId;
	FString OriginInstanceId;
	FString RequestorId;
	bool bDrivenLocal = false;
	bool bDrivenHasCtrl = false;
	int32 Role = 0;
	int32 RemoteRole = 0;
	bool bIgnoreMove = false;
	bool bIgnoreLook = false;
	bool bUnlocked = true;
	FVector Loc = FVector::ZeroVector;
	bool bPcLocal = false;
};

/**
 * Hub drive trace: which HTTP/WS port received the command, what the caller
 * intended to drive, and whether that pawn is this process's local human,
 * a headless spawned bot, or another window's net-human (alert).
 */
namespace CLHubDriveTrace
{
	void StampListen(const TSharedPtr<FJsonObject>& Root, int32 ListenPort, const TCHAR* Recv);
	void StampViaRpc(const TSharedPtr<FJsonObject>& Root, int32 FromListenPort);
	void FillAndLog(UCLLobbySubsystem* Lobby, const TSharedPtr<FJsonObject>& Root, const FGuid* FallbackSeat,
		ACLPlayerController* RecvPC = nullptr);
	void ApplyToJson(const TSharedRef<FJsonObject>& Out);
	const FCLHubDriveSnap& Last();
	void NotePlayerController(ACLPlayerController* PC, const TSharedPtr<FJsonObject>& Root);
}
