#include "AI/CLBotBookTrace.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Core/CLLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

static TAutoConsoleVariable<int32> CVarDLBotBookTrace(
	TEXT("dl.BotBook.Trace"),
	0,
	TEXT("If 1, log BotBook leaf start/settle and *-to envelope phases to LogCallingBotBook."));

bool CLBotBookTrace::IsOn()
{
	if (CVarDLBotBookTrace.GetValueOnGameThread() > 0)
	{
		return true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("BotBookTrace")))
	{
		return true;
	}
	bool bIni = false;
	if (GConfig)
	{
		GConfig->GetBool(TEXT("/Script/Calling.CLBotBookSettings"), TEXT("bTraceHandlers"), bIni, GGameIni);
	}
	return bIni;
}

void CLBotBookTrace::LeafStart(const TCHAR* Verb, const TCHAR* NodeId, FName Marker, const FVector& From, const FVector& Goal)
{
	if (!IsOn())
	{
		return;
	}
	const float DistXY = FVector::Dist2D(From, Goal);
	UE_LOG(LogCallingBotBook, Display,
		TEXT("leaf start verb=%s node=%s marker=%s from=(%.0f,%.0f,%.0f) goal=(%.0f,%.0f,%.0f) distXY=%.0f dZ=%.0f"),
		Verb, NodeId, *Marker.ToString(), From.X, From.Y, From.Z, Goal.X, Goal.Y, Goal.Z, DistXY, Goal.Z - From.Z);
}

void CLBotBookTrace::LeafSettle(const TCHAR* Verb, const TCHAR* NodeId, const TCHAR* Outcome, float Elapsed,
	const FVector& Loc, const FVector& Goal)
{
	if (!IsOn())
	{
		return;
	}
	UE_LOG(LogCallingBotBook, Display,
		TEXT("leaf settle verb=%s node=%s outcome=%s elapsed=%.2f loc=(%.0f,%.0f,%.0f) distXY=%.0f dZ=%.0f"),
		Verb, NodeId, Outcome, Elapsed, Loc.X, Loc.Y, Loc.Z, FVector::Dist2D(Loc, Goal), Goal.Z - Loc.Z);
}

void CLBotBookTrace::ExecStart(const TCHAR* Mode, const FVector& From, const FVector& Goal, const FCLNavAbilityBox& Box,
	const TCHAR* Sub, int32 Jumps)
{
	if (!IsOn())
	{
		return;
	}
	UE_LOG(LogCallingBotBook, Display,
		TEXT("exec start mode=%s sub=%s jumps=%d from=(%.0f,%.0f,%.0f) goal=(%.0f,%.0f,%.0f) distXY=%.0f dZ=%.0f boxXY=%.0f..%.0f boxZ=%.0f..%.0f release=%.0f coast=%.0f"),
		Mode, Sub, Jumps, From.X, From.Y, From.Z, Goal.X, Goal.Y, Goal.Z,
		FVector::Dist2D(From, Goal), Goal.Z - From.Z,
		Box.MinDistXY, Box.MaxDistXY, Box.MinDeltaZ, Box.MaxDeltaZ, Box.ReleaseDistXY, Box.CoastXY);
}

void CLBotBookTrace::Phase(const TCHAR* Mode, const TCHAR* What, float Elapsed, const FVector& Loc, const FVector& Goal,
	const FVector& Vel, const TCHAR* Extra)
{
	if (!IsOn())
	{
		return;
	}
	UE_LOG(LogCallingBotBook, Display,
		TEXT("exec phase mode=%s what=%s t=%.2f loc=(%.0f,%.0f,%.0f) distXY=%.0f dZ=%.0f velXY=%.0f vz=%.0f %s"),
		Mode, What, Elapsed, Loc.X, Loc.Y, Loc.Z, FVector::Dist2D(Loc, Goal), Goal.Z - Loc.Z,
		Vel.Size2D(), Vel.Z, Extra);
}

void CLBotBookTrace::Sample(const TCHAR* Mode, const TCHAR* Phase, float Elapsed, float Dt, const FVector& Loc,
	const FVector& Vel, const FVector& Goal)
{
	if (!IsOn())
	{
		return;
	}
	UE_LOG(LogCallingBotBook, Display,
		TEXT("exec sample mode=%s phase=%s t=%.2f dt=%.3f loc=(%.0f,%.0f,%.0f) velXY=%.0f vz=%.0f distXY=%.0f dZ=%.0f"),
		Mode, Phase, Elapsed, Dt, Loc.X, Loc.Y, Loc.Z, Vel.Size2D(), Vel.Z,
		FVector::Dist2D(Loc, Goal), Goal.Z - Loc.Z);
}

void CLBotBookTrace::VelInterval(const TCHAR* Mode, const TCHAR* Phase, float MinXY, float MaxXY, float MeanXY,
	float MinVz, float MaxVz)
{
	if (!IsOn())
	{
		return;
	}
	UE_LOG(LogCallingBotBook, Display,
		TEXT("exec vel mode=%s phase=%s xy=%.0f..%.0f mean=%.0f vz=%.0f..%.0f"),
		Mode, Phase, MinXY, MaxXY, MeanXY, MinVz, MaxVz);
}

void CLBotBookTrace::Miss(const TCHAR* Mode, const TCHAR* Result, const TCHAR* Phase, const FVector& Loc, const FVector& Goal,
	const FCLNavAbilityBox& Box, float ReleaseDist, bool bOnPad)
{
	if (!IsOn())
	{
		return;
	}
	const float DistXY = FVector::Dist2D(Loc, Goal);
	const float DeltaZ = Goal.Z - Loc.Z;
	UE_LOG(LogCallingBotBook, Display,
		TEXT("exec miss mode=%s result=%s phase=%s distXY=%.0f dZ=%.0f missXY=%.0f missZHigh=%.0f missZLow=%.0f releaseMiss=%.0f onPad=%d"),
		Mode, Result, Phase, DistXY, DeltaZ,
		DistXY - Box.MaxDistXY, DeltaZ - Box.MaxDeltaZ, Box.MinDeltaZ - DeltaZ,
		DistXY - ReleaseDist, bOnPad ? 1 : 0);
}

void CLBotBookTrace::GotoArm(const TCHAR* Arm, const FVector& From, const FVector& Dest, bool bPartial, int32 PathPts)
{
	if (!IsOn())
	{
		return;
	}
	UE_LOG(LogCallingBotBook, Display,
		TEXT("goto arm=%s from=(%.0f,%.0f,%.0f) dest=(%.0f,%.0f,%.0f) distXY=%.0f dZ=%.0f partial=%d pts=%d"),
		Arm, From.X, From.Y, From.Z, Dest.X, Dest.Y, Dest.Z,
		FVector::Dist2D(From, Dest), Dest.Z - From.Z, bPartial ? 1 : 0, PathPts);
}
