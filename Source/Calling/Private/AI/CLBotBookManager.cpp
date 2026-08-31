#include "AI/CLBotBookManager.h"
#include "AI/CLBotBookParser.h"
#include "AI/CLBotBookTrace.h"
#include "AI/CLBotVerbs.h"
#include "AI/CLTaskMarker.h"
#include "Game/CLSeatMotor.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLGameModeBase.h"
#include "Game/CLGameStateBase.h"
#include "Player/CLPlayerController.h"
#include "Player/CLPlayerCharacter.h"
#include "Game/CLErrorBoundary.h"
#include "Core/CLError.h"
#include "Nav/CLAgentNavProbe.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Core/CLLog.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

namespace
{
	FGuid EnemySeat(UCLParticipantSeat* Seat, UCLLobbySubsystem* Lobby)
	{
		if (!Seat || !Lobby)
		{
			return FGuid();
		}
		const APawn* MinePawn = Seat->GetDrivenPawn();
		UCLParticipantSeat* Identity = Seat;
		if (Seat->GetDriveSeatId().IsValid())
		{
			if (UCLParticipantSeat* Drive = Lobby->FindSeat(Seat->GetDriveSeatId()))
			{
				Identity = Drive;
			}
		}
		const ECLPvpTeam Mine = Identity->GetTeam();
		for (UCLParticipantSeat* Other : Lobby->GetSeats())
		{
			if (!Other || Other == Seat)
			{
				continue;
			}
			APawn* OtherPawn = Other->GetDrivenPawn();
			if (!OtherPawn || OtherPawn == MinePawn)
			{
				continue;
			}
			if (Mine != ECLPvpTeam::Unassigned && Other->GetTeam() == Mine)
			{
				continue;
			}
			return Other->GetSeatId();
		}
		return FGuid();
	}

	float DistXY(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
	}

	bool IsLiveOccupyAlias(FName Id)
	{
		return Id == FName(TEXT("live_occupy")) || Id == FName(TEXT("liveOccupy"))
			|| Id == FName(TEXT("live_shrine")) || Id == FName(TEXT("liveShrine"));
	}

	bool IsLiveOrbitAlias(FName Id)
	{
		return Id == FName(TEXT("live_orbit")) || Id == FName(TEXT("liveOrbit"));
	}

	FName LiveOccupyId(UWorld* World)
	{
		if (ACLGameStateBase* GS = World ? World->GetGameState<ACLGameStateBase>() : nullptr)
		{
			return GS->GetLiveShrine();
		}
		return NAME_None;
	}

	void CollectOrbitCrumbs(UWorld* World, FName Occupy, TArray<ACLTaskMarker*>& Out)
	{
		Out.Reset();
		if (!World || Occupy.IsNone())
		{
			return;
		}
		for (int32 i = 0; i < 32; ++i)
		{
			const FName Id(*FString::Printf(TEXT("%s_orbit_%d"), *Occupy.ToString(), i));
			if (ACLTaskMarker* M = ACLTaskMarker::FindById(World, Id))
			{
				Out.Add(M);
			}
			else
			{
				break;
			}
		}
	}

	ACLTaskMarker* PeekOrbitCrumb(UWorld* World, FName Occupy, int32 Index)
	{
		TArray<ACLTaskMarker*> Crumbs;
		if (Occupy.IsNone())
		{
			Occupy = LiveOccupyId(World);
		}
		CollectOrbitCrumbs(World, Occupy, Crumbs);
		if (Crumbs.Num() == 0)
		{
			return nullptr;
		}
		const int32 Idx = Index >= 0 ? Index % Crumbs.Num() : 0;
		return Crumbs[Idx];
	}

	bool OutcomeIn(ECLBotOutcome Last, const TArray<FString>& Names)
	{
		const FString Cur = CLBotOutcomeName(Last);
		for (const FString& N : Names)
		{
			if (N.Equals(Cur, ESearchCase::IgnoreCase)
				|| (N.Equals(TEXT("Success"), ESearchCase::IgnoreCase) && Last == ECLBotOutcome::Success)
				|| (N.Equals(TEXT("GoodEnough"), ESearchCase::IgnoreCase) && Last == ECLBotOutcome::GoodEnough)
				|| (N.Equals(TEXT("Fail"), ESearchCase::IgnoreCase) && Last == ECLBotOutcome::Fail)
				|| (N.Equals(TEXT("Skipped"), ESearchCase::IgnoreCase) && Last == ECLBotOutcome::Skipped))
			{
				return true;
			}
		}
		return false;
	}
}

FString UCLBotBookManager::BooksDir()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("BotBooks"));
}

void UCLBotBookManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
}

void UCLBotBookManager::Deinitialize()
{
	Runtimes.Empty();
	Catalog.Empty();
	Super::Deinitialize();
}

void UCLBotBookManager::LoadCatalog()
{
	Catalog.Empty();
	IFileManager& FM = IFileManager::Get();
	TArray<FString> Files;
	FM.FindFiles(Files, *FPaths::Combine(BooksDir(), TEXT("*.puml")), true, false);
	for (const FString& File : Files)
	{
		const FString Path = FPaths::Combine(BooksDir(), File);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::NonDeterministic, TEXT("botbook_io"), Path));
			continue;
		}
		FCLBotBook Book;
		FString Err;
		const FCLStatus St = FCLBotBookParser::Parse(Text, false, Book, Err);
		if (!St.IsOk())
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("botbook_load"), FString::Printf(TEXT("%s: %s"), *File, *Err)));
			continue;
		}
		if (Book.Name.IsNone())
		{
			Book.Name = FName(*FPaths::GetBaseFilename(File));
		}
		FString VerbErr;
		if (!ValidateLeaves(Book.Body, VerbErr))
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::User, TEXT("botbook_unknown_verb"), FString::Printf(TEXT("%s: %s"), *File, *VerbErr)));
			continue;
		}
		Catalog.Add(Book.Name, MakeShared<FCLBotBook>(MoveTemp(Book)));
	}
	for (const TPair<FName, TSharedPtr<FCLBotBook>>& Pair : Catalog)
	{
		TSet<FName> Path;
		TSet<FName> Done;
		FString CycleErr;
		if (!DetectCycles(Pair.Key, Path, Done, CycleErr))
		{
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("botbook_cycle"), CycleErr));
		}
	}
	UE_LOG(LogCalling, Display, TEXT("BotBook catalog loaded %d"), Catalog.Num());
}

const FCLBotBook* UCLBotBookManager::FindBook(FName Name) const
{
	if (const TSharedPtr<FCLBotBook>* Found = Catalog.Find(Name))
	{
		return Found->Get();
	}
	return nullptr;
}

TSharedPtr<UCLBotBookManager::FRuntime> UCLBotBookManager::EnsureRuntime(const FGuid& SeatId)
{
	TSharedPtr<FRuntime>& Rt = Runtimes.FindOrAdd(SeatId);
	if (!Rt.IsValid())
	{
		Rt = MakeShared<FRuntime>();
	}
	return Rt;
}

bool UCLBotBookManager::IsExecuting(const FRuntime& Rt) const
{
	return CurrentStmt(Rt) != nullptr;
}

bool UCLBotBookManager::CatalogAlreadyLiveOrQueued(const FRuntime& Rt, FName Name) const
{
	if (Name.IsNone())
	{
		return false;
	}
	if (IsExecuting(Rt) && Rt.ActiveName == Name)
	{
		return true;
	}
	for (const FQueuedBook& Q : Rt.Queue)
	{
		if (!Q.Jit.IsValid() && Q.CatalogName == Name)
		{
			return true;
		}
	}
	return false;
}

bool UCLBotBookManager::NoteCancelStorm(FRuntime& Rt, bool bStrategic, FString& OutError)
{
	const double Now = FPlatformTime::Seconds();
	const float Window = bStrategic ? 4.f : 8.f;
	const int32 Limit = bStrategic ? 8 : 3;
	TArray<double>& Times = bStrategic ? Rt.StrategicCancelAt : Rt.PreemptCancelAt;
	Times.RemoveAll([Now, Window](double T) { return (Now - T) > Window; });
	Times.Add(Now);
	if (Times.Num() < Limit)
	{
		return true;
	}
	OutError = bStrategic ? TEXT("botbook_cancel_storm") : TEXT("botbook_preempt_storm");
	UCLErrorBoundary::ReportStatic(this, FCLError::Make(
		ECLErrorKind::User,
		OutError,
		bStrategic
			? TEXT("situation branchBotBook faster than replan (8 in 4s)")
			: TEXT("append cancelled a live book (3 in 8s)")));
	return false;
}

bool UCLBotBookManager::ParseBranchCause(const FString& Cause, ECLBotBookBranchCause& OutCause, FString& OutError) const
{
	const FString C = Cause.TrimStartAndEnd().ToLower();
	if (C.IsEmpty())
	{
		OutCause = ECLBotBookBranchCause::Invalid;
		OutError = TEXT("missing_branch_cause");
		return false;
	}
	if (C == TEXT("execution") || C == TEXT("failure") || C == TEXT("fail"))
	{
		OutCause = ECLBotBookBranchCause::Execution;
		return true;
	}
	if (C == TEXT("situation") || C == TEXT("combat") || C == TEXT("personality") || C == TEXT("strategic"))
	{
		OutCause = ECLBotBookBranchCause::Situation;
		return true;
	}
	OutCause = ECLBotBookBranchCause::Invalid;
	OutError = TEXT("invalid_branch_cause");
	return false;
}

bool UCLBotBookManager::NoteBranchCause(UCLParticipantSeat* Seat, FRuntime& Rt, ECLBotBookBranchCause Cause, FString& OutError)
{
	const TCHAR* CauseName = Cause == ECLBotBookBranchCause::Execution ? TEXT("execution") : TEXT("situation");
	FString Node;
	if (const FCLBotStmt* Stmt = CurrentStmt(Rt))
	{
		Node = Stmt->Id;
	}
	Rt.LastBranchCause = CauseName;
	Rt.LastBranchNode = Node;
	FBranchObs& Obs = BranchObs.FindOrAdd(Seat ? Seat->GetSeatId() : FGuid());
	Obs.LastCause = CauseName;
	Obs.LastNode = Node;
	Obs.LastBook = Rt.ActiveName.ToString();
	if (Cause == ECLBotBookBranchCause::Execution)
	{
		Rt.ExecutionFails++;
		Obs.ExecutionFails++;
		if (!Obs.bReportedExecution)
		{
			Obs.bReportedExecution = true;
			Rt.bReportedExecution = true;
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("botbook_execution"),
				FString::Printf(TEXT("branch cause=execution book=%s node=%s — bot failed the book (not outside factors)"),
					*Obs.LastBook, *Node)));
			NoteMatchEvent(Seat, TEXT("botbook_execution"),
				FString::Printf(TEXT("cause=execution book=%s node=%s"), *Obs.LastBook, *Node), true);
		}
		return true;
	}
	return NoteCancelStorm(Rt, true, OutError);
}

bool UCLBotBookManager::EnqueueBook(FRuntime& Rt, FName CatalogName, TSharedPtr<FCLBotBook> Jit, FString& OutError)
{
	if (Rt.Queue.Num() >= MaxQueuedBooks)
	{
		OutError = TEXT("botbook_queue");
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::User, TEXT("botbook_queue"), TEXT("too many queued BotBooks behind the live one")));
		return false;
	}
	FQueuedBook Q;
	Q.CatalogName = CatalogName;
	Q.Jit = MoveTemp(Jit);
	Rt.Queue.Add(MoveTemp(Q));
	return true;
}

bool UCLBotBookManager::BeginNow(UCLParticipantSeat* Seat, FRuntime& Rt, const FCLBotBook& Book, FString& OutError)
{
	if (!IsExecuting(Rt))
	{
		if (UCLSeatMotor* Motor = Seat ? Seat->GetSeatMotor() : nullptr)
		{
			Motor->CancelGoto();
		}
		if (UCLRemoteAgentSeatMotor* Remote = Seat ? Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()) : nullptr)
		{
			Remote->MarkReply();
		}
	}
	Rt.FallbackIndex = 0;
	if (!PushBook(Rt, Book, OutError))
	{
		return false;
	}
	NoteMatchEvent(Seat, TEXT("botbook_append"), Book.Name.ToString(), false);
	return true;
}

bool UCLBotBookManager::ContinueAfterExhausted(UCLParticipantSeat* Seat, FRuntime& Rt)
{
	FString Err;
	if (StartQueued(Seat, Rt, Err))
	{
		return true;
	}
	if (!Rt.OnStop.IsNone())
	{
		if (const FCLBotBook* Book = FindBook(Rt.OnStop))
		{
			return BeginNow(Seat, Rt, *Book, Err);
		}
	}
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	ACLGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<ACLGameStateBase>() : nullptr;
	const bool bMatchLive = GS && GS->GetModeResult().Equals(TEXT("in_progress"), ESearchCase::IgnoreCase);
	if (Char && Char->IsCombatAlive() && bMatchLive && !Rt.OnRespawn.IsNone())
	{
		NoteFollowAlert(Seat, TEXT("botbook_idle"),
			TEXT("book exhausted while match in_progress (onRespawn set, no onStop/queue)"));
	}
	ClearSeat(Seat->GetSeatId());
	return false;
}

bool UCLBotBookManager::StartQueued(UCLParticipantSeat* Seat, FRuntime& Rt, FString& OutError)
{
	if (Rt.Queue.Num() == 0)
	{
		return false;
	}
	const FQueuedBook Q = Rt.Queue[0];
	Rt.Queue.RemoveAt(0);
	if (Q.Jit.IsValid())
	{
		Rt.JitBook = Q.Jit;
		Rt.JitHold.Add(Q.Jit);
		Rt.bJit = true;
		return BeginNow(Seat, Rt, *Rt.JitBook, OutError);
	}
	const FCLBotBook* Book = FindBook(Q.CatalogName);
	if (!Book)
	{
		OutError = TEXT("unknown_botbook");
		return false;
	}
	Rt.bJit = false;
	return BeginNow(Seat, Rt, *Book, OutError);
}

bool UCLBotBookManager::HasRuntime(const FGuid& SeatId) const
{
	const TSharedPtr<FRuntime>* Found = Runtimes.Find(SeatId);
	return Found && Found->IsValid() && ((*Found)->Stack.Num() > 0 || (*Found)->Queue.Num() > 0
		|| !(*Found)->OnStop.IsNone());
}

void UCLBotBookManager::ClearSeat(const FGuid& SeatId)
{
	if (TSharedPtr<FRuntime>* Found = Runtimes.Find(SeatId))
	{
		for (FFrame& Fr : (*Found)->Stack)
		{
			Fr.Verb.Reset();
		}
	}
	Runtimes.Remove(SeatId);
}

bool UCLBotBookManager::PushBook(FRuntime& Rt, const FCLBotBook& Book, FString& OutError)
{
	if (Rt.Stack.Num() >= MaxBookStack)
	{
		OutError = TEXT("botbook_stack");
		if (!Rt.bReportedStack)
		{
			Rt.bReportedStack = true;
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("botbook_stack"),
				FString::Printf(TEXT("BotBook stack %d (max %d); append must not preempt the live book"),
					Rt.Stack.Num(), MaxBookStack)));
		}
		return false;
	}
	FFrame Fr;
	Fr.BookName = Book.Name;
	Fr.Seq = &Book.Body;
	Fr.Index = 0;
	Rt.Stack.Add(MoveTemp(Fr));
	Rt.ActiveName = Book.Name;
	if (Rt.Fallbacks.Num() == 0)
	{
		Rt.Fallbacks = Book.Fallbacks;
	}
	if (Rt.OnRespawn.IsNone())
	{
		Rt.OnRespawn = Book.OnRespawn;
	}
	if (Rt.OnStop.IsNone())
	{
		Rt.OnStop = Book.OnStop;
	}
	return true;
}

bool UCLBotBookManager::AppendCatalog(UCLParticipantSeat* Seat, const FString& BookName, FString& OutError)
{
	if (!Seat)
	{
		OutError = TEXT("no_seat");
		return false;
	}
	const FCLBotBook* Book = FindBook(FName(*BookName));
	if (!Book)
	{
		OutError = TEXT("unknown_botbook");
		return false;
	}
	TSharedPtr<FRuntime> Rt = EnsureRuntime(Seat->GetSeatId());
	if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
	{
		Remote->MarkReply();
	}
	if (IsExecuting(*Rt))
	{
		if (CatalogAlreadyLiveOrQueued(*Rt, Book->Name))
		{
			return true;
		}
		return EnqueueBook(*Rt, Book->Name, nullptr, OutError);
	}
	return BeginNow(Seat, *Rt, *Book, OutError);
}

bool UCLBotBookManager::AppendJit(UCLParticipantSeat* Seat, const FString& Puml, FString& OutError)
{
	if (!Seat)
	{
		OutError = TEXT("no_seat");
		return false;
	}
	FCLBotBook Book;
	const FCLStatus St = FCLBotBookParser::Parse(Puml, true, Book, OutError);
	if (!St.IsOk())
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("botbook_jit"), OutError));
		return false;
	}
	if (!ValidateLeaves(Book.Body, OutError))
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("botbook_unknown_verb"), OutError));
		return false;
	}
	Book.bJit = true;
	TSharedPtr<FRuntime> Rt = EnsureRuntime(Seat->GetSeatId());
	if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
	{
		Remote->MarkReply();
	}
	TSharedPtr<FCLBotBook> Held = MakeShared<FCLBotBook>(MoveTemp(Book));
	if (IsExecuting(*Rt))
	{
		return EnqueueBook(*Rt, NAME_None, Held, OutError);
	}
	Rt->JitBook = Held;
	Rt->JitHold.Add(Held);
	Rt->bJit = true;
	return BeginNow(Seat, *Rt, *Rt->JitBook, OutError);
}

bool UCLBotBookManager::BranchCatalog(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& BookName, const FString& Cause, FString& OutError)
{
	if (!Seat)
	{
		OutError = TEXT("no_seat");
		return false;
	}
	ECLBotBookBranchCause Parsed = ECLBotBookBranchCause::Invalid;
	if (!ParseBranchCause(Cause, Parsed, OutError))
	{
		return false;
	}
	const FCLBotBook* Book = FindBook(FName(*BookName));
	if (!Book)
	{
		OutError = TEXT("unknown_botbook");
		return false;
	}
	TSharedPtr<FRuntime> Rt = EnsureRuntime(Seat->GetSeatId());
	bool bPast = true;
	if (Rt->Stack.Num() > 0)
	{
		const FFrame& Top = Rt->Stack.Last();
		if (Top.Seq)
		{
			for (int32 i = Top.Index; i < Top.Seq->Num(); ++i)
			{
				if (!AfterId.IsEmpty() && (*Top.Seq)[i].Id == AfterId)
				{
					bPast = false;
					break;
				}
			}
			if (Offset >= 0 && Offset < (Top.Seq->Num() - Top.Index))
			{
				bPast = false;
			}
		}
	}
	if (bPast)
	{
		return AppendCatalog(Seat, BookName, OutError);
	}
	if (!NoteBranchCause(Seat, *Rt, Parsed, OutError))
	{
		return false;
	}
	StopLeaf(*Rt, Seat);
	Rt->Stack.Last().Index = Rt->Stack.Last().Seq->Num();
	return BeginNow(Seat, *Rt, *Book, OutError);
}

bool UCLBotBookManager::BranchJit(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& Puml, const FString& Cause, FString& OutError)
{
	ECLBotBookBranchCause Parsed = ECLBotBookBranchCause::Invalid;
	if (!ParseBranchCause(Cause, Parsed, OutError))
	{
		return false;
	}
	TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat ? Seat->GetSeatId() : FGuid());
	const bool bPast = !Found || !Found->IsValid() || !IsExecuting(**Found);
	(void)AfterId;
	(void)Offset;
	if (bPast)
	{
		return AppendJit(Seat, Puml, OutError);
	}
	if (!NoteBranchCause(Seat, **Found, Parsed, OutError))
	{
		return false;
	}
	StopLeaf(**Found, Seat);
	(*Found)->Stack.Last().Index = (*Found)->Stack.Last().Seq->Num();
	FCLBotBook Book;
	const FCLStatus St = FCLBotBookParser::Parse(Puml, true, Book, OutError);
	if (!St.IsOk())
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("botbook_jit"), OutError));
		return false;
	}
	if (!ValidateLeaves(Book.Body, OutError))
	{
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::User, TEXT("botbook_unknown_verb"), OutError));
		return false;
	}
	Book.bJit = true;
	(*Found)->JitBook = MakeShared<FCLBotBook>(MoveTemp(Book));
	(*Found)->JitHold.Add((*Found)->JitBook);
	(*Found)->bJit = true;
	return BeginNow(Seat, **Found, *(*Found)->JitBook, OutError);
}

void UCLBotBookManager::NotifyRespawn(UCLParticipantSeat* Seat)
{
	if (!Seat)
	{
		return;
	}
	TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat->GetSeatId());
	FName Next = NAME_None;
	if (Found && Found->IsValid())
	{
		Next = (*Found)->OnRespawn;
	}
	ClearSeat(Seat->GetSeatId());
	if (!Next.IsNone())
	{
		FString Err;
		AppendCatalog(Seat, Next.ToString(), Err);
	}
}

const FCLBotStmt* UCLBotBookManager::CurrentStmt(const FRuntime& Rt) const
{
	if (Rt.Stack.Num() == 0)
	{
		return nullptr;
	}
	const FFrame& Top = Rt.Stack.Last();
	if (!Top.Seq || !Top.Seq->IsValidIndex(Top.Index))
	{
		return nullptr;
	}
	return &(*Top.Seq)[Top.Index];
}

bool UCLBotBookManager::EvalPredicate(const FCLBotPredicate& Pred, UCLParticipantSeat* Seat, const FFrame* Frame, ECLBotOutcome Last) const
{
	if (Pred.IsEmpty())
	{
		return true;
	}
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	UWorld* World = Char ? Char->GetWorld() : nullptr;
	const FString Name = Pred.Name.ToLower();
	auto Cmp = [&](float Lhs) -> bool
	{
		const float Rhs = FCString::Atof(*Pred.Value);
		if (Pred.Op == TEXT(">")) { return Lhs > Rhs; }
		if (Pred.Op == TEXT(">=")) { return Lhs >= Rhs; }
		if (Pred.Op == TEXT("<")) { return Lhs < Rhs; }
		if (Pred.Op == TEXT("<=")) { return Lhs <= Rhs; }
		if (Pred.Op == TEXT("!=")) { return !FMath::IsNearlyEqual(Lhs, Rhs); }
		return FMath::IsNearlyEqual(Lhs, Rhs);
	};

	if (Name == TEXT("output"))
	{
		return OutcomeIn(Last, Pred.OrValues.Num() ? Pred.OrValues : TArray<FString>{Pred.Value});
	}
	if (Name == TEXT("alive"))
	{
		return Char && Char->IsCombatAlive();
	}
	if (Name == TEXT("navtiles"))
	{
		return Cmp(static_cast<float>(CLAgentNavProbe::NavTileCount(World)));
	}
	if (Name == TEXT("hasfocus"))
	{
		return Frame && true;
	}
	if (Name == TEXT("air"))
	{
		const UCLCombatMovementComponent* Move = Char ? Char->GetCombatMovement() : nullptr;
		return Move && !Move->IsMovingOnGround();
	}
	if (Name == TEXT("sliding"))
	{
		return Char && Char->GetCombatMovement() && Char->GetCombatMovement()->IsSliding();
	}
	if (Name == TEXT("diving"))
	{
		return Char && Char->GetCombatMovement() && Char->GetCombatMovement()->IsDiveReported();
	}
	if (Name == TEXT("hasmarker"))
	{
		FName Want(*Pred.Value);
		if (IsLiveOccupyAlias(Want))
		{
			if (ACLGameStateBase* GS = World ? World->GetGameState<ACLGameStateBase>() : nullptr)
			{
				if (!GS->GetLiveShrine().IsNone())
				{
					Want = GS->GetLiveShrine();
				}
			}
		}
		if (IsLiveOrbitAlias(Want))
		{
			TArray<ACLTaskMarker*> Crumbs;
			CollectOrbitCrumbs(World, LiveOccupyId(World), Crumbs);
			return Crumbs.Num() > 0;
		}
		return ACLTaskMarker::FindById(World, Want) != nullptr;
	}
	if (Name == TEXT("z"))
	{
		return Char && Cmp(Char->GetActorLocation().Z);
	}
	if (Name == TEXT("distxy"))
	{
		if (!Char)
		{
			return false;
		}
		FVector Goal = Char->GetActorLocation();
		if (UCLSeatMotor* Motor = Seat->GetSeatMotor())
		{
			if (Motor->IsGotoActive())
			{
				Goal = Motor->GetGotoGoal();
			}
		}
		if (Frame && Frame->Verb)
		{
			(void)Frame;
		}
		return Cmp(DistXY(Char->GetActorLocation(), Goal));
	}
	if (Name == TEXT("enemydistxy"))
	{
		// Missing / dead enemy is far so `< N` takes the occupy side, not a stall.
		const float Far = TNumericLimits<float>::Max();
		if (!Char || !Seat)
		{
			return Cmp(Far);
		}
		UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
		const FGuid OtherId = EnemySeat(Seat, Lobby);
		APawn* OtherPawn = Lobby && OtherId.IsValid() ? Lobby->GetDrivenPawn(OtherId) : nullptr;
		const ACLPlayerCharacter* OtherChar = Cast<ACLPlayerCharacter>(OtherPawn);
		if (!OtherChar || !OtherChar->IsCombatAlive())
		{
			return Cmp(Far);
		}
		return Cmp(DistXY(Char->GetActorLocation(), OtherChar->GetActorLocation()));
	}
	if (Name == TEXT("true") || Pred.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	return false;
}

bool UCLBotBookManager::StartLeaf(FRuntime& Rt, UCLParticipantSeat* Seat, FString& OutError)
{
	FFrame& Top = Rt.Stack.Last();
	const FCLBotStmt* Stmt = CurrentStmt(Rt);
	if (!Stmt || Stmt->Kind != ECLBotStmtKind::Leaf)
	{
		OutError = TEXT("not_leaf");
		return false;
	}
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	UCLSeatMotor* Motor = Seat ? Seat->GetSeatMotor() : nullptr;
	FCLBotVerbContext Ctx;
	Ctx.World = Char ? Char->GetWorld() : nullptr;
	Ctx.Seat = Seat;
	Ctx.Char = Char;
	Ctx.Motor = Motor;
	Ctx.Leaf = &Stmt->Leaf;
	Ctx.FocusSeat = Rt.FocusSeat;
	if (const FString* Marker = Stmt->Leaf.Params.Find(TEXT("marker")))
	{
		FName MarkerId(**Marker);
		if (IsLiveOccupyAlias(MarkerId))
		{
			if (ACLGameStateBase* GS = Ctx.World ? Ctx.World->GetGameState<ACLGameStateBase>() : nullptr)
			{
				if (!GS->GetLiveShrine().IsNone())
				{
					MarkerId = GS->GetLiveShrine();
				}
			}
		}
		if (IsLiveOrbitAlias(MarkerId))
		{
			const FName Occupy = LiveOccupyId(Ctx.World);
			if (Occupy != Rt.LiveOrbitOccupy)
			{
				Rt.LiveOrbitOccupy = Occupy;
				Rt.LiveOrbitIndex = -1;
			}
			TArray<ACLTaskMarker*> Crumbs;
			CollectOrbitCrumbs(Ctx.World, Occupy, Crumbs);
			ACLTaskMarker* Pick = nullptr;
			if (Crumbs.Num() > 0)
			{
				if (Rt.LiveOrbitIndex < 0)
				{
					int32 Best = 0;
					float BestDist = -1.f;
					UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
					APawn* FocusPawn = nullptr;
					if (Lobby && Rt.FocusSeat.IsValid())
					{
						FocusPawn = Lobby->GetDrivenPawn(Rt.FocusSeat);
					}
					if (FocusPawn)
					{
						for (int32 i = 0; i < Crumbs.Num(); ++i)
						{
							const float D = DistXY(Crumbs[i]->GetActorLocation(), FocusPawn->GetActorLocation());
							if (D > BestDist)
							{
								BestDist = D;
								Best = i;
							}
						}
					}
					Rt.LiveOrbitIndex = Best;
				}
				else
				{
					Rt.LiveOrbitIndex = (Rt.LiveOrbitIndex + 1) % Crumbs.Num();
				}
				Pick = Crumbs[Rt.LiveOrbitIndex];
				MarkerId = Pick->Id;
			}
			Ctx.MarkerId = MarkerId;
			if (Pick)
			{
				Ctx.Goal = Pick->GetActorLocation();
			}
			else
			{
				UE_LOG(LogCalling, Warning, TEXT("BotBook missing live_orbit for %s"), *Occupy.ToString());
				if (Stmt->Leaf.Verb == FName(TEXT("goto")))
				{
					NoteFollowAlert(Seat, TEXT("botbook_missing_marker"),
						FString::Printf(TEXT("goto live_orbit occupy=%s"), *Occupy.ToString()));
					return false;
				}
			}
		}
		else
		{
		Ctx.MarkerId = MarkerId;
		if (ACLTaskMarker* Mark = ACLTaskMarker::FindById(Ctx.World, Ctx.MarkerId))
		{
			Ctx.Goal = Mark->GetActorLocation();
		}
		else
		{
			UE_LOG(LogCalling, Warning, TEXT("BotBook missing marker %s"), *MarkerId.ToString());
			if (Stmt->Leaf.Verb == FName(TEXT("goto")))
			{
				NoteFollowAlert(Seat, TEXT("botbook_missing_marker"),
					FString::Printf(TEXT("goto marker=%s not in world"), *MarkerId.ToString()));
				return false;
			}
		}
		}
	}
	else if (Stmt->Leaf.Params.Contains(TEXT("x")) || Stmt->Leaf.Params.Contains(TEXT("y")) || Stmt->Leaf.Params.Contains(TEXT("z")))
	{
		const FString* Xs = Stmt->Leaf.Params.Find(TEXT("x"));
		const FString* Ys = Stmt->Leaf.Params.Find(TEXT("y"));
		const FString* Zs = Stmt->Leaf.Params.Find(TEXT("z"));
		Ctx.Goal = FVector(
			Xs ? FCString::Atof(**Xs) : 0.f,
			Ys ? FCString::Atof(**Ys) : 0.f,
			Zs ? FCString::Atof(**Zs) : 0.f);
	}
	if (Stmt->Leaf.Verb == FName(TEXT("setfocus")) || Stmt->Leaf.Verb == FName(TEXT("trackfocus")))
	{
		const FString* Focus = Stmt->Leaf.Params.Find(TEXT("focus"));
		if (Focus && Focus->Equals(TEXT("enemy"), ESearchCase::IgnoreCase))
		{
			UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
			Rt.FocusSeat = EnemySeat(Seat, Lobby);
			Ctx.FocusSeat = Rt.FocusSeat;
		}
	}
	for (const FName& W : Stmt->Leaf.WhileVerbs)
	{
		const FString L = W.ToString().ToLower();
		if (L != TEXT("trackfocus") && L != TEXT("setfocus") && L != TEXT("maintainads") && L != TEXT("fire"))
		{
			OutError = TEXT("unknown_while");
			NoteFollowAlert(Seat, TEXT("botbook_unknown_while"), W.ToString());
			return false;
		}
	}
	Top.Verb = CLMakeBotVerb(Stmt->Leaf.Verb);
	if (!Top.Verb.IsValid())
	{
		OutError = TEXT("unknown_verb");
		NoteFollowAlert(Seat, TEXT("botbook_unknown_verb"), Stmt->Leaf.Verb.ToString());
		return false;
	}
	Top.bLeafStarted = true;
	Top.LeafElapsed = 0.f;
	Top.GoodEnoughHold = 0.f;
	Top.bSawGoodEnough = false;
	Top.GotoNoStickHold = 0.f;
	Top.StillAnchor = FVector::ZeroVector;
	Top.PawnStillSeconds = 0.f;
	Top.Verb->Start(Ctx);
	if (Stmt->Leaf.Verb == FName(TEXT("goto")) && Char
		&& DistXY(Char->GetActorLocation(), Ctx.Goal) > 300.f
		&& (!Ctx.Motor || !Ctx.Motor->IsGotoActive()))
	{
		NoteFollowAlert(Seat, TEXT("botbook_goto_start_failed"),
			FString::Printf(TEXT("StartGoto did not activate DistXY=%.0f"), DistXY(Char->GetActorLocation(), Ctx.Goal)));
	}
	CLBotBookTrace::LeafStart(*Stmt->Leaf.Verb.ToString(), *Stmt->Id, Ctx.MarkerId,
		Char ? Char->GetActorLocation() : FVector::ZeroVector, Ctx.Goal);
	return true;
}

void UCLBotBookManager::StopLeaf(FRuntime& Rt, UCLParticipantSeat* Seat)
{
	if (Rt.Stack.Num() == 0)
	{
		return;
	}
	FFrame& Top = Rt.Stack.Last();
	if (Top.Verb.IsValid())
	{
		FCLBotVerbContext Ctx;
		ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
		Ctx.World = Char ? Char->GetWorld() : nullptr;
		Ctx.Seat = Seat;
		Ctx.Char = Char;
		Ctx.Motor = Seat ? Seat->GetSeatMotor() : nullptr;
		Top.Verb->Stop(Ctx);
		Top.Verb.Reset();
		if (Char)
		{
			Char->ClearAgentIntent();
		}
	}
	Top.bLeafStarted = false;
}

ECLBotOutcome UCLBotBookManager::TickLeaf(FRuntime& Rt, float DeltaSeconds, UCLParticipantSeat* Seat)
{
	FFrame& Top = Rt.Stack.Last();
	const FCLBotStmt* Stmt = CurrentStmt(Rt);
	if (!Stmt)
	{
		return ECLBotOutcome::Fail;
	}
	ACLPlayerCharacter* Char = Seat ? Cast<ACLPlayerCharacter>(Seat->GetDrivenPawn()) : nullptr;
	FCLBotVerbContext Ctx;
	Ctx.World = Char ? Char->GetWorld() : nullptr;
	Ctx.Seat = Seat;
	Ctx.Char = Char;
	Ctx.Motor = Seat ? Seat->GetSeatMotor() : nullptr;
	Ctx.Leaf = &Stmt->Leaf;
	Ctx.FocusSeat = Rt.FocusSeat;
	Ctx.Elapsed = Top.LeafElapsed;
	const bool bGotoVerb = Stmt->Leaf.Verb == FName(TEXT("goto"));
	if (bGotoVerb && Ctx.Motor && Ctx.Motor->IsGotoActive())
	{
		Ctx.Goal = Ctx.Motor->GetGotoGoal();
	}
	else if (const FString* Marker = Stmt->Leaf.Params.Find(TEXT("marker")))
	{
		FName MarkerId(**Marker);
		if (IsLiveOccupyAlias(MarkerId))
		{
			MarkerId = LiveOccupyId(Ctx.World);
			if (ACLTaskMarker* Mark = ACLTaskMarker::FindById(Ctx.World, MarkerId))
			{
				Ctx.Goal = Mark->GetActorLocation();
			}
		}
		else if (IsLiveOrbitAlias(MarkerId))
		{
			if (ACLTaskMarker* Pick = PeekOrbitCrumb(Ctx.World, Rt.LiveOrbitOccupy, Rt.LiveOrbitIndex))
			{
				Ctx.Goal = Pick->GetActorLocation();
			}
		}
		else if (ACLTaskMarker* Mark = ACLTaskMarker::FindById(Ctx.World, MarkerId))
		{
			Ctx.Goal = Mark->GetActorLocation();
		}
	}
	else if (Stmt->Leaf.Params.Contains(TEXT("x")) || Stmt->Leaf.Params.Contains(TEXT("y")) || Stmt->Leaf.Params.Contains(TEXT("z")))
	{
		const FString* Xs = Stmt->Leaf.Params.Find(TEXT("x"));
		const FString* Ys = Stmt->Leaf.Params.Find(TEXT("y"));
		const FString* Zs = Stmt->Leaf.Params.Find(TEXT("z"));
		Ctx.Goal = FVector(
			Xs ? FCString::Atof(**Xs) : 0.f,
			Ys ? FCString::Atof(**Ys) : 0.f,
			Zs ? FCString::Atof(**Zs) : 0.f);
	}

	Top.LeafElapsed += DeltaSeconds;
	if (Top.Verb.IsValid())
	{
		Top.Verb->Tick(DeltaSeconds, Ctx);
	}
	if (bGotoVerb && Char)
	{
		const UCLCombatMovementComponent* MoveComp = Char->GetCombatMovement();
		const bool bFlight = (MoveComp && MoveComp->IsDiving())
			|| (Ctx.Motor && Ctx.Motor->GetGotoDriver().bFlight);
		const FVector Loc = Char->GetActorLocation();
		const float Dist = DistXY(Loc, Ctx.Goal);
		if (Top.StillAnchor.IsNearlyZero())
		{
			Top.StillAnchor = Loc;
		}
		if (FVector::Dist(Loc, Top.StillAnchor) > 80.f || Char->GetVelocity().Size() > 80.f)
		{
			Top.StillAnchor = Loc;
			Top.PawnStillSeconds = 0.f;
		}
		else
		{
			Top.PawnStillSeconds += DeltaSeconds;
		}
		const FVector2D IntentStick = Char->GetAgentMove();
		const FVector2D RecastStick = Ctx.Motor ? Ctx.Motor->GetGotoDriver().LastMoveXY : FVector2D::ZeroVector;
		float SettleCm = 300.f;
		if (Stmt->Leaf.GoodEnough.Name.Equals(TEXT("distXY"), ESearchCase::IgnoreCase))
		{
			SettleCm = FCString::Atof(*Stmt->Leaf.GoodEnough.Value);
		}
		else if (Stmt->Leaf.Success.Name.Equals(TEXT("distXY"), ESearchCase::IgnoreCase))
		{
			SettleCm = FCString::Atof(*Stmt->Leaf.Success.Value);
		}
		const bool bFar = Dist > SettleCm;
		const bool bZeroIntent = IntentStick.Size() < 0.05f;
		const bool bZeroRecast = RecastStick.Size() < 0.05f;
		if (!Char->IsCombatAlive())
		{
			Top.StillAnchor = FVector::ZeroVector;
			Top.PawnStillSeconds = 0.f;
			Top.GotoNoStickHold = 0.f;
		}
		else if (bFar && !bFlight)
		{
				const float WpStuck = Ctx.Motor ? Ctx.Motor->GetGotoDriver().StuckSeconds : 0.f;
				if (Top.PawnStillSeconds >= 0.5f)
				{
					const FCLAgentGotoDriver& G = Ctx.Motor ? Ctx.Motor->GetGotoDriver() : FCLAgentGotoDriver();
					const bool bAir = MoveComp && !MoveComp->IsMovingOnGround();
					const bool bDive = MoveComp && MoveComp->IsDiving();
					float GoodEnoughCm = -1.f;
					if (Stmt->Leaf.GoodEnough.Name.Equals(TEXT("distXY"), ESearchCase::IgnoreCase))
					{
						GoodEnoughCm = FCString::Atof(*Stmt->Leaf.GoodEnough.Value);
					}
					const FVector Wp = G.Path.IsValidIndex(G.Index) ? G.Path[G.Index] : G.Goal;
					const float VelXY = Char->GetVelocity().Size2D();
					NoteFollowAlert(Seat, TEXT("botbook_goto_loc_still"),
						FString::Printf(
							TEXT("pawn still %.1fs goalDist=%.0f wpStuck=%.1fs z=%.0f velXY=%.0f air=%d dive=%d flight=%d fwd=%s/%.0f reason=%s move=(%.2f,%.2f) blocked=%d ge=%.0f inGe=%d idx=%d/%d wp=(%.0f,%.0f,%.0f) repathLeft=%d look=%.0f ctrl=%.0f"),
							Top.PawnStillSeconds, Dist, WpStuck,
							Loc.Z, VelXY, bAir ? 1 : 0, bDive ? 1 : 0, bFlight ? 1 : 0,
							*G.FwdKind.ToString(), G.FwdDist, *G.SteerReason.ToString(),
							G.LastMoveXY.X, G.LastMoveXY.Y, G.bMoveBlocked ? 1 : 0,
							GoodEnoughCm, (GoodEnoughCm > 0.f && Dist <= GoodEnoughCm) ? 1 : 0,
							G.Index, G.Path.Num(), Wp.X, Wp.Y, Wp.Z, G.RepathLeft,
							Char->GetControlRotation().Yaw, Char->GetActorRotation().Yaw),
						false);
				}
				if (Stmt->Leaf.WhileVerbs.Num() > 0 && bZeroIntent && !bZeroRecast)
			{
				Top.GotoNoStickHold += DeltaSeconds;
				if (Top.GotoNoStickHold >= 0.5f)
				{
					NoteFollowAlert(Seat, TEXT("botbook_goto_stick_clobber"),
						FString::Printf(TEXT("while: overwrote Recast stick DistXY=%.0f"), Dist));
				}
			}
			else if (bZeroIntent && bZeroRecast)
			{
				Top.GotoNoStickHold += DeltaSeconds;
				if (Top.GotoNoStickHold >= 0.5f)
				{
					NoteFollowAlert(Seat, TEXT("botbook_goto_no_stick"),
						FString::Printf(TEXT("goto live DistXY=%.0f stick=0"), Dist));
				}
			}
			else
			{
				Top.GotoNoStickHold = 0.f;
			}
		}
		else
		{
			Top.GotoNoStickHold = 0.f;
		}
	}

	const FCLBotBook* Book = FindBook(Top.BookName);
	if (!Book && Rt.JitBook.IsValid() && Rt.JitBook->Name == Top.BookName)
	{
		Book = Rt.JitBook.Get();
	}
	const float TryFor = Stmt->Leaf.TrySuccessFor >= 0.f
		? Stmt->Leaf.TrySuccessFor
		: (Book ? Book->DefaultTrySuccessFor : 2.5f);
	const float Timeout = Stmt->Leaf.FailTimeout > 0.f ? Stmt->Leaf.FailTimeout : 8.f;

	auto PredOk = [&](const FCLBotPredicate& P) -> bool
	{
		if (P.IsEmpty())
		{
			if (Stmt->Leaf.Verb == FName(TEXT("goto")) && Char)
			{
				if (DistXY(Char->GetActorLocation(), Ctx.Goal) > 150.f)
				{
					return false;
				}
				const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
				return Move && Move->IsMovingOnGround() && !Move->IsDiving()
					&& CLNavAbility::StandingOnGoalFloor(Char->GetActorLocation(), Ctx.Goal);
			}
			if (Stmt->Leaf.Verb == FName(TEXT("setfocus")) || Stmt->Leaf.Verb == FName(TEXT("trackfocus")))
			{
				return Rt.FocusSeat.IsValid() || Top.LeafElapsed > 0.15f;
			}
			if (Stmt->Leaf.Verb == FName(TEXT("useabilityself")) || Stmt->Leaf.Verb == FName(TEXT("useabilityfocus")))
			{
				return Top.LeafElapsed > 0.2f;
			}
			if (Stmt->Leaf.Verb == FName(TEXT("wait")) || Stmt->Leaf.Verb == FName(TEXT("idle")))
			{
				return Top.LeafElapsed >= Timeout;
			}
			if (Stmt->Leaf.WhileVerbs.Num() > 0)
			{
				return false;
			}
			return Top.LeafElapsed > 0.35f;
		}
		if (P.Name.Equals(TEXT("distXY"), ESearchCase::IgnoreCase) && Char)
		{
			const float D = DistXY(Char->GetActorLocation(), Ctx.Goal);
			const float Rhs = FCString::Atof(*P.Value);
			bool bDistOk = (P.Op == TEXT(">") || P.Op == TEXT(">=")) ? (D >= Rhs) : (D <= Rhs);
			const FString Verb = Stmt->Leaf.Verb.ToString();
			const bool bToLeaf = (Verb.Equals(TEXT("airDive"), ESearchCase::IgnoreCase)
					|| Verb.Equals(TEXT("jump"), ESearchCase::IgnoreCase)
					|| Verb.Equals(TEXT("slide"), ESearchCase::IgnoreCase)
					|| Verb.Equals(TEXT("dash"), ESearchCase::IgnoreCase)
					|| Verb.Equals(TEXT("dodge"), ESearchCase::IgnoreCase))
				&& (Stmt->Leaf.Params.Contains(TEXT("marker")) || Stmt->Leaf.Params.Contains(TEXT("x")));
			const bool bGoto = Verb.Equals(TEXT("goto"), ESearchCase::IgnoreCase);
			if ((bToLeaf || bGoto) && bDistOk)
			{
				const UCLCombatMovementComponent* Move = Char->GetCombatMovement();
				return Move && Move->IsMovingOnGround() && !Move->IsDiving()
					&& CLNavAbility::StandingOnGoalFloor(Char->GetActorLocation(), Ctx.Goal);
			}
			return bDistOk;
		}
		if (P.Name.Equals(TEXT("z"), ESearchCase::IgnoreCase) && Char)
		{
			const float Z = Char->GetActorLocation().Z;
			const float Rhs = FCString::Atof(*P.Value);
			if (P.Op == TEXT(">")) { return Z > Rhs; }
			if (P.Op == TEXT(">=")) { return Z >= Rhs; }
			if (P.Op == TEXT("<")) { return Z < Rhs; }
			if (P.Op == TEXT("<=")) { return Z <= Rhs; }
			return FMath::IsNearlyEqual(Z, Rhs);
		}
		if (P.Name.Equals(TEXT("air"), ESearchCase::IgnoreCase))
		{
			const UCLCombatMovementComponent* Move = Char ? Char->GetCombatMovement() : nullptr;
			const bool bAir = Move && !Move->IsMovingOnGround();
			const bool bWant = !P.Value.Equals(TEXT("false"), ESearchCase::IgnoreCase);
			return bAir == bWant;
		}
		if (P.Name.Equals(TEXT("sliding"), ESearchCase::IgnoreCase))
		{
			return Char && Char->GetCombatMovement() && Char->GetCombatMovement()->IsSliding();
		}
		if (P.Name.Equals(TEXT("diving"), ESearchCase::IgnoreCase))
		{
			return Char && Char->GetCombatMovement() && Char->GetCombatMovement()->IsDiveReported();
		}
		if (P.Name.Equals(TEXT("notair"), ESearchCase::IgnoreCase) || (P.Name.Equals(TEXT("air"), ESearchCase::IgnoreCase) && P.Value.Equals(TEXT("false"))))
		{
			const UCLCombatMovementComponent* Move = Char ? Char->GetCombatMovement() : nullptr;
			return Move && Move->IsMovingOnGround();
		}
		return EvalPredicate(P, Seat, &Top, Top.LastOutcome);
	};

	const bool bSuccess = PredOk(Stmt->Leaf.Success);
	const bool bGood = PredOk(Stmt->Leaf.GoodEnough);
	const bool bImpossible = Top.Verb.IsValid() && Top.Verb->SuccessImpossible(Ctx) && !bSuccess;

	const FVector Loc = Char ? Char->GetActorLocation() : FVector::ZeroVector;
	auto Settle = [&](ECLBotOutcome Out, const TCHAR* Why) -> ECLBotOutcome
	{
		UE_LOG(LogCalling, Display, TEXT("BotBook settle %s leaf=%s"), Why, *Stmt->Id);
		CLBotBookTrace::LeafSettle(*Stmt->Leaf.Verb.ToString(), *Stmt->Id, CLBotOutcomeName(Out),
			Top.LeafElapsed, Loc, Ctx.Goal);
		return Out;
	};

	if (bSuccess)
	{
		return Settle(ECLBotOutcome::Success, TEXT("success"));
	}
	if (bImpossible && bGood)
	{
		return Settle(ECLBotOutcome::GoodEnough, TEXT("goodEnoughProbe"));
	}
	if (bImpossible && !bGood)
	{
		return Settle(ECLBotOutcome::Fail, TEXT("failProbe"));
	}
	if (bGood)
	{
		if (!Top.bSawGoodEnough)
		{
			Top.bSawGoodEnough = true;
			Top.GoodEnoughHold = 0.f;
		}
		Top.GoodEnoughHold += DeltaSeconds;
		if (Top.GoodEnoughHold >= TryFor)
		{
			return Settle(ECLBotOutcome::GoodEnough, TEXT("goodEnoughAfterHold"));
		}
	}
	if (Top.LeafElapsed >= Timeout)
	{
		return Settle(ECLBotOutcome::Fail, TEXT("failTimeout"));
	}
	return ECLBotOutcome::None;
}

bool UCLBotBookManager::TryFallback(FRuntime& Rt, FString& OutError)
{
	while (Rt.FallbackIndex < Rt.Fallbacks.Num())
	{
		const FName Name = Rt.Fallbacks[Rt.FallbackIndex++];
		if (const FCLBotBook* Book = FindBook(Name))
		{
			Rt.Stack.Reset();
			return PushBook(Rt, *Book, OutError);
		}
	}
	return false;
}

bool UCLBotBookManager::TickSeat(float DeltaSeconds, UCLParticipantSeat* Seat)
{
	if (!Seat)
	{
		return false;
	}
	if (APawn* Driven = Seat->GetDrivenPawn())
	{
		if (!Driven->IsLocallyControlled())
		{
			if (HasRuntime(Seat->GetSeatId()))
			{
				NoteFollowAlert(Seat, TEXT("botbook_not_local"),
					TEXT("live BotBook on a pawn this process does not locally control"));
			}
			return false;
		}
	}
	TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat->GetSeatId());
	if (!Found || !Found->IsValid())
	{
		return false;
	}
	if (FBranchObs* Obs = BranchObs.Find(Seat->GetSeatId()))
	{
		Obs->FollowAlertLive.Reset();
	}
	FRuntime& Rt = *Found->Get();
	if (Rt.Stack.Num() == 0)
	{
		if (!ContinueAfterExhausted(Seat, Rt))
		{
			return false;
		}
	}
	if (Rt.Stack.Num() > MaxBookStack && !Rt.bReportedStack)
	{
		Rt.bReportedStack = true;
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("botbook_stack"),
			FString::Printf(TEXT("BotBook stack %d exceeds tick guard %d"), Rt.Stack.Num(), MaxBookStack)));
	}
	UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	if (!Rt.FocusSeat.IsValid())
	{
		Rt.FocusSeat = EnemySeat(Seat, Lobby);
	}

	const int32 Guard = MaxBookStack;
	for (int32 Step = 0; Step < Guard && Rt.Stack.Num() > 0; ++Step)
	{
		FFrame& Top = Rt.Stack.Last();
		if (!Top.Seq)
		{
			Rt.Stack.Pop();
			continue;
		}
		if (!Top.Seq->IsValidIndex(Top.Index))
		{
			Rt.Stack.Pop();
			if (Rt.Stack.Num() > 0)
			{
				Rt.Stack.Last().Index++;
			}
			continue;
		}
		const FCLBotStmt& Stmt = (*Top.Seq)[Top.Index];
		if (Stmt.Kind == ECLBotStmtKind::Stop)
		{
			Rt.Stack.Pop();
			continue;
		}
		if (Stmt.Kind == ECLBotStmtKind::If)
		{
			const ECLBotOutcome Last = Top.LastOutcome;
			const bool bYes = EvalPredicate(Stmt.IfPred, Seat, &Top, Last);
			FFrame Child;
			Child.BookName = Top.BookName;
			if (bYes)
			{
				Child.Seq = &Stmt.ThenBody;
			}
			else if (Stmt.ElseBody.Num() > 0)
			{
				Child.Seq = &Stmt.ElseBody;
			}
			else
			{
				if (Stmt.ElseTag == TEXT("skip"))
				{
					Top.LastOutcome = ECLBotOutcome::Skipped;
					UE_LOG(LogCalling, Display, TEXT("BotBook skipped if=%s"), *Stmt.Id);
				}
				else if (Stmt.ElseTag == TEXT("fail"))
				{
					Top.LastOutcome = ECLBotOutcome::Fail;
					FString Err;
					StopLeaf(Rt, Seat);
					if (!TryFallback(Rt, Err))
					{
						ClearSeat(Seat->GetSeatId());
						return false;
					}
					continue;
				}
				Top.Index++;
				continue;
			}
			Child.Index = 0;
			Top.Index++;
			Rt.Stack.Add(MoveTemp(Child));
			continue;
		}
		if (Stmt.Kind == ECLBotStmtKind::Ref)
		{
			const FCLBotBook* Ref = FindBook(Stmt.RefName);
			if (!Ref)
			{
				UE_LOG(LogCalling, Warning, TEXT("BotBook missing ref %s"), *Stmt.RefName.ToString());
				Top.LastOutcome = ECLBotOutcome::Fail;
				Top.Index++;
				continue;
			}
			Top.Index++;
			FString Err;
			PushBook(Rt, *Ref, Err);
			continue;
		}
		if (Stmt.Kind == ECLBotStmtKind::Leaf)
		{
			if (!Top.bLeafStarted)
			{
				FString Err;
				if (!StartLeaf(Rt, Seat, Err))
				{
					UE_LOG(LogCalling, Display, TEXT("BotBook settle failProbe leaf=%s err=%s"), *Stmt.Id, *Err);
					StopLeaf(Rt, Seat);
					Top.LastOutcome = ECLBotOutcome::Fail;
					Top.Index++;
					continue;
				}
			}
			const ECLBotOutcome Out = TickLeaf(Rt, DeltaSeconds, Seat);
			if (Out == ECLBotOutcome::None)
			{
				if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
				{
					Remote->MarkReply();
				}
				return true;
			}
			StopLeaf(Rt, Seat);
			Top.LastOutcome = Out;
			if (Out == ECLBotOutcome::Fail)
			{
				FString Err;
				if (TryFallback(Rt, Err))
				{
					continue;
				}
				Top.Index++;
				continue;
			}
			Top.Index++;
			continue;
		}
		Top.Index++;
	}
	if (Rt.Stack.Num() == 0)
	{
		return ContinueAfterExhausted(Seat, Rt);
	}
	return true;
}

void UCLBotBookManager::CollectRemaining(const FRuntime& Rt, TArray<FString>& OutIds) const
{
	for (int32 f = Rt.Stack.Num() - 1; f >= 0; --f)
	{
		const FFrame& Fr = Rt.Stack[f];
		if (!Fr.Seq)
		{
			continue;
		}
		for (int32 i = Fr.Index; i < Fr.Seq->Num(); ++i)
		{
			OutIds.Add((*Fr.Seq)[i].Id);
		}
	}
}

TSharedRef<FJsonObject> UCLBotBookManager::MakeSeatBotJson(const FGuid& SeatId) const
{
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	auto WriteObs = [&]()
	{
		if (const FBranchObs* O = BranchObs.Find(SeatId))
		{
			Obj->SetStringField(TEXT("lastBranchCause"), O->LastCause);
			Obj->SetStringField(TEXT("lastBranchNodeId"), O->LastNode);
			Obj->SetStringField(TEXT("lastBranchBook"), O->LastBook);
			Obj->SetNumberField(TEXT("executionFails"), O->ExecutionFails);
			Obj->SetBoolField(TEXT("executionError"), O->ExecutionFails >= 1);
			Obj->SetStringField(TEXT("followAlert"), O->FollowAlertLive);
			Obj->SetStringField(TEXT("lastFollowAlert"), O->LastFollowAlert.IsEmpty() ? O->FollowAlert : O->LastFollowAlert);
			Obj->SetBoolField(TEXT("followed"), O->FollowAlertLive.IsEmpty());
		}
		else
		{
			Obj->SetStringField(TEXT("followAlert"), TEXT(""));
			Obj->SetStringField(TEXT("lastFollowAlert"), TEXT(""));
			Obj->SetBoolField(TEXT("followed"), true);
		}
	};
	auto WriteEnemy = [&]()
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UCLLobbySubsystem* Lobby = GI->GetSubsystem<UCLLobbySubsystem>())
			{
				if (UCLParticipantSeat* Seat = Lobby->FindSeat(SeatId))
				{
					const FGuid Enemy = EnemySeat(Seat, Lobby);
					if (APawn* Mine = Seat->GetDrivenPawn())
					{
						if (APawn* Other = Lobby->GetDrivenPawn(Enemy))
						{
							Obj->SetNumberField(TEXT("enemyDistXY"), DistXY(Mine->GetActorLocation(), Other->GetActorLocation()));
							Obj->SetNumberField(TEXT("enemyX"), Other->GetActorLocation().X);
							Obj->SetNumberField(TEXT("enemyY"), Other->GetActorLocation().Y);
						}
					}
				}
			}
		}
	};
	const TSharedPtr<FRuntime>* Found = Runtimes.Find(SeatId);
	if (!Found || !Found->IsValid() || ((*Found)->Stack.Num() == 0 && (*Found)->Queue.Num() == 0))
	{
		Obj->SetBoolField(TEXT("jit"), false);
		WriteEnemy();
		WriteObs();
		return Obj;
	}
	const FRuntime& Rt = *Found->Get();
	Obj->SetStringField(TEXT("name"), Rt.ActiveName.ToString());
	Obj->SetBoolField(TEXT("jit"), Rt.bJit);
	if (const FCLBotStmt* Stmt = CurrentStmt(Rt))
	{
		Obj->SetStringField(TEXT("nodeId"), Stmt->Id);
		if (Stmt->Kind == ECLBotStmtKind::Leaf)
		{
			Obj->SetStringField(TEXT("verb"), Stmt->Leaf.Verb.ToString());
			TArray<TSharedPtr<FJsonValue>> WhileArr;
			for (const FName& W : Stmt->Leaf.WhileVerbs)
			{
				WhileArr.Add(MakeShared<FJsonValueString>(W.ToString()));
			}
			Obj->SetArrayField(TEXT("whiles"), WhileArr);
		}
	}
	if (Rt.Stack.Num() > 0)
	{
		const FFrame& Top = Rt.Stack.Last();
		Obj->SetNumberField(TEXT("leafElapsed"), Top.LeafElapsed);
		Obj->SetNumberField(TEXT("pawnStill"), Top.PawnStillSeconds);
	}
	if (Rt.FocusSeat.IsValid())
	{
		Obj->SetStringField(TEXT("focusSeat"), Rt.FocusSeat.ToString(EGuidFormats::DigitsWithHyphens));
	}
	WriteEnemy();
	TArray<TSharedPtr<FJsonValue>> StackArr;
	const int32 StackN = Rt.Stack.Num();
	const int32 StackShow = FMath::Min(StackN, 8);
	for (int32 i = StackN - StackShow; i < StackN; ++i)
	{
		StackArr.Add(MakeShared<FJsonValueString>(Rt.Stack[i].BookName.ToString()));
	}
	Obj->SetArrayField(TEXT("stack"), StackArr);
	Obj->SetNumberField(TEXT("stackLen"), StackN);
	TArray<FString> Remain;
	CollectRemaining(Rt, Remain);
	TArray<TSharedPtr<FJsonValue>> RemArr;
	const int32 RemShow = FMath::Min(Remain.Num(), 12);
	for (int32 i = 0; i < RemShow; ++i)
	{
		RemArr.Add(MakeShared<FJsonValueString>(Remain[i]));
	}
	Obj->SetArrayField(TEXT("remaining"), RemArr);
	Obj->SetNumberField(TEXT("remainingLen"), Remain.Num());
	Obj->SetNumberField(TEXT("queueLen"), Rt.Queue.Num());
	TArray<TSharedPtr<FJsonValue>> QArr;
	for (const FQueuedBook& Q : Rt.Queue)
	{
		QArr.Add(MakeShared<FJsonValueString>(Q.Jit.IsValid() ? TEXT("jit") : Q.CatalogName.ToString()));
	}
	Obj->SetArrayField(TEXT("queued"), QArr);
	WriteObs();
	return Obj;
}

void UCLBotBookManager::FillStateJson(TSharedRef<FJsonObject> Root, const FGuid& SeatId) const
{
	Root->SetObjectField(TEXT("botBook"), MakeSeatBotJson(SeatId));
}

bool UCLBotBookManager::CollectRefs(const TArray<FCLBotStmt>& Body, TArray<FName>& OutRefs)
{
	for (const FCLBotStmt& S : Body)
	{
		if (S.Kind == ECLBotStmtKind::Ref)
		{
			OutRefs.Add(S.RefName);
		}
		CollectRefs(S.ThenBody, OutRefs);
		CollectRefs(S.ElseBody, OutRefs);
	}
	return true;
}

bool UCLBotBookManager::DetectCycles(FName From, TSet<FName>& Path, TSet<FName>& Done, FString& OutError) const
{
	if (Done.Contains(From))
	{
		return true;
	}
	if (Path.Contains(From))
	{
		OutError = From.ToString();
		return false;
	}
	Path.Add(From);
	if (const FCLBotBook* Book = FindBook(From))
	{
		TArray<FName> Refs;
		CollectRefs(Book->Body, Refs);
		for (const FName& Ref : Refs)
		{
			if (!DetectCycles(Ref, Path, Done, OutError))
			{
				return false;
			}
		}
	}
	Path.Remove(From);
	Done.Add(From);
	return true;
}

bool UCLBotBookManager::HasXyzGoto(const TArray<FCLBotStmt>& Body)
{
	for (const FCLBotStmt& S : Body)
	{
		if (S.Kind == ECLBotStmtKind::Leaf && S.Leaf.Verb == FName(TEXT("goto")) && S.Leaf.Params.Contains(TEXT("x")) && !S.Leaf.Params.Contains(TEXT("marker")))
		{
			return true;
		}
		if (HasXyzGoto(S.ThenBody) || HasXyzGoto(S.ElseBody))
		{
			return true;
		}
	}
	return false;
}

void UCLBotBookManager::NoteFollowAlert(UCLParticipantSeat* Seat, const TCHAR* Code, const FString& Detail, bool bCountExecution)
{
	if (!Seat || !Code)
	{
		return;
	}
	FBranchObs& Obs = BranchObs.FindOrAdd(Seat->GetSeatId());
	const FString Key(Code);
	Obs.FollowAlertLive = Key;
	if (Obs.LastFollowAlert.IsEmpty())
	{
		Obs.LastFollowAlert = Key;
	}
	if (Obs.ReportedFollow.Contains(Key))
	{
		return;
	}
	Obs.ReportedFollow.Add(Key);
	Obs.FollowAlert = Key;
	Obs.LastFollowAlert = Key;
	if (bCountExecution)
	{
		Obs.ExecutionFails++;
		Obs.bReportedExecution = true;
	}
	if (TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat->GetSeatId()))
	{
		if (Found->IsValid() && bCountExecution)
		{
			(*Found)->ExecutionFails++;
			(*Found)->bReportedExecution = true;
		}
	}
	const FString Msg = Detail.IsEmpty() ? Key : FString::Printf(TEXT("%s: %s"), Code, *Detail);
	UE_LOG(LogCalling, Error, TEXT("BotBook followAlert %s"), *Msg);
	UCLErrorBoundary::ReportStatic(this, FCLError::Make(ECLErrorKind::NonDeterministic, Key, Msg));
	NoteMatchEvent(Seat, Code, Detail, true);
}

void UCLBotBookManager::NoteMatchEvent(UCLParticipantSeat* Seat, const TCHAR* Code, const FString& Detail, bool bFailMatch)
{
	if (!Code)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	APawn* Pawn = Seat ? Seat->GetDrivenPawn() : nullptr;
	const FVector Loc = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	FString SeatStr;
	FString Book;
	if (Seat)
	{
		SeatStr = Seat->GetSeatId().ToString(EGuidFormats::DigitsWithHyphens);
		if (const TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat->GetSeatId()))
		{
			if (Found->IsValid())
			{
				Book = (*Found)->ActiveName.ToString();
			}
		}
	}
	if (World->GetNetMode() == NM_Client)
	{
		APlayerController* PC = Seat ? Seat->GetBoundController() : nullptr;
		if (!PC)
		{
			PC = World->GetFirstPlayerController();
		}
		if (ACLPlayerController* CLPC = Cast<ACLPlayerController>(PC))
		{
			CLPC->ServerBotBookEvent(FString(Code), Detail, SeatStr, Book, Loc.X, Loc.Y, bFailMatch);
		}
		return;
	}
	FCLMatchEvent E;
	E.Code = Code;
	E.Detail = Detail;
	E.Seat = SeatStr;
	E.Book = Book;
	E.X = Loc.X;
	E.Y = Loc.Y;
	E.Time = World->GetTimeSeconds();
	if (ACLGameStateBase* GS = World->GetGameState<ACLGameStateBase>())
	{
		GS->AppendMatchEvent(E);
	}
	if (bFailMatch)
	{
		if (ACLPvpGameMode* Pvp = World->GetAuthGameMode<ACLPvpGameMode>())
		{
			Pvp->FailBook(FString(Code));
		}
		else if (ACLRaidGameMode* Raid = World->GetAuthGameMode<ACLRaidGameMode>())
		{
			Raid->FailBook(FString(Code));
		}
	}
}

bool UCLBotBookManager::ValidateLeaves(const TArray<FCLBotStmt>& Body, FString& OutError)
{
	for (const FCLBotStmt& S : Body)
	{
		if (S.Kind == ECLBotStmtKind::Leaf)
		{
			if (!CLMakeBotVerb(S.Leaf.Verb).IsValid())
			{
				OutError = FString::Printf(TEXT("unknown_verb %s"), *S.Leaf.Verb.ToString());
				return false;
			}
			for (const FName& W : S.Leaf.WhileVerbs)
			{
				const FString L = W.ToString().ToLower();
				if (L != TEXT("trackfocus") && L != TEXT("setfocus") && L != TEXT("maintainads") && L != TEXT("fire"))
				{
					OutError = FString::Printf(TEXT("unknown_while %s"), *W.ToString());
					return false;
				}
			}
		}
		if (!ValidateLeaves(S.ThenBody, OutError) || !ValidateLeaves(S.ElseBody, OutError))
		{
			return false;
		}
	}
	return true;
}
