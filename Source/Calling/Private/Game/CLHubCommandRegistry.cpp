#include "Game/CLLobbyTypes.h"
#include "Game/CLHubCommandRegistry.h"
#include "Game/CLAgentCodec.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLControllerPlaybook.h"

using namespace CLAgentCodec;

namespace
{
	TSharedRef<FJsonObject> Fail(const FString& Error)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), false);
		Out->SetStringField(TEXT("error"), Error);
		return Out;
	}

	TSharedRef<FJsonObject> Ok()
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), true);
		return Out;
	}

	FGuid ResolveSeat(UCLLobbySubsystem* Lobby, const TSharedPtr<FJsonObject>& Root, const FGuid* FallbackSeat)
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
		return SeatId;
	}
}

TSharedRef<FJsonObject> FCLHubCommandRegistry::Dispatch(
	UCLLobbySubsystem* Lobby,
	const TSharedPtr<FJsonObject>& Root,
	FGuid* FallbackSeat)
{
	if (!Lobby)
	{
		return Fail(TEXT("no_lobby"));
	}
	if (!Root.IsValid())
	{
		return Fail(TEXT("invalid_json"));
	}

	const FString Type = JsonStr(Root, TEXT("type")).ToLower();
	if (Type.IsEmpty())
	{
		return Fail(TEXT("missing_type"));
	}

	if (Type == TEXT("join"))
	{
		FString Error;
		UCLParticipantSeat* Seat = Lobby->JoinRemoteAgent(
			JsonStr(Root, TEXT("displayName"), TEXT("agent")),
			JsonBool(Root, TEXT("headless")),
			Error,
			JsonStr(Root, TEXT("kind")));
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), Seat != nullptr);
		if (Seat)
		{
			Out->SetStringField(TEXT("seatId"), GuidStr(Seat->GetSeatId()));
			Out->SetBoolField(TEXT("headless"), JsonBool(Root, TEXT("headless")));
			Out->SetStringField(TEXT("kind"), Seat->GetPlaybook() ? Seat->GetPlaybook()->GetKindId() : TEXT("none"));
			if (FallbackSeat)
			{
				*FallbackSeat = Seat->GetSeatId();
			}
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
		if (!Lobby->FindSeat(SeatId))
		{
			return Fail(TEXT("no_seat"));
		}
		TSharedRef<FJsonObject> Out = Ok();
		Out->SetStringField(TEXT("seatId"), GuidStr(SeatId));
		if (FallbackSeat)
		{
			*FallbackSeat = SeatId;
		}
		return Out;
	}

	if (Type == TEXT("ready"))
	{
		const FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), Lobby->SetReady(SeatId, JsonBool(Root, TEXT("ready"), true)));
		return Out;
	}

	if (Type == TEXT("go"))
	{
		const FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		const UCLParticipantSeat* Seat = SeatId.IsValid() ? Lobby->FindSeat(SeatId) : Lobby->FindLocalSeat();
		if (!Seat || !Seat->IsHost())
		{
			return Fail(TEXT("host_only"));
		}
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), Lobby->RequestGo());
		return Out;
	}

	if (Type == TEXT("mindcontrol"))
	{
		FString Error;
		const bool bOk = Lobby->MindControl(
			ParseGuid(JsonStr(Root, TEXT("seatId"))),
			ParseGuid(JsonStr(Root, TEXT("targetSeatId"))),
			Error);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
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
			SeatId = Lobby->GetLastJoinedSeatId();
		}
		const FString TeamText = JsonStr(Root, TEXT("team")).ToLower();
		ECLPvpTeam Team = ECLPvpTeam::Unassigned;
		if (TeamText == TEXT("blue"))
		{
			Team = ECLPvpTeam::Blue;
		}
		else if (TeamText == TEXT("red"))
		{
			Team = ECLPvpTeam::Red;
		}
		const bool bOk = Lobby->SetTeam(SeatId, Team, Error);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), bOk);
		if (!bOk)
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	if (Type == TEXT("plan"))
	{
		TArray<FCLAgentStep> Steps;
		bool bRemainder = false;
		ParseSteps(Root, Steps, bRemainder);
		const FGuid SeatId = ResolveSeat(Lobby, Root, FallbackSeat);
		FString Error;
		const bool bOk = Lobby->QueuePlan(SeatId, Steps, bRemainder, Error);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), bOk);
		if (!bOk)
		{
			Out->SetStringField(TEXT("error"), Error);
		}
		return Out;
	}

	if (Type == TEXT("goto"))
	{
		const FGuid SeatId = ResolveSeat(Lobby, Root, FallbackSeat);
		FString Error;
		const FVector Dest(JsonNum(Root, TEXT("x")), JsonNum(Root, TEXT("y")), JsonNum(Root, TEXT("z")));
		const bool bOk = Lobby->StartGoto(SeatId, Dest, Error);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		Out->SetBoolField(TEXT("ok"), bOk);
		if (bOk)
		{
			UCLParticipantSeat* Seat = Lobby->FindSeat(SeatId.IsValid() ? SeatId : Lobby->GetLastJoinedSeatId());
			if (const UCLRemoteAgentPlaybook* Remote = Seat ? Cast<UCLRemoteAgentPlaybook>(Seat->GetPlaybook()) : nullptr)
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
		const FGuid SeatId = ParseGuid(JsonStr(Root, TEXT("seatId")));
		const bool bOk = Lobby->SetViewSeat(SeatId, Error);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
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

	return Fail(TEXT("unknown_type"));
}
