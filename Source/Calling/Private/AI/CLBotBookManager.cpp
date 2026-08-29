#include "AI/CLBotBookManager.h"
#include "AI/CLBotBookParser.h"
#include "AI/CLBotBookTrace.h"
#include "AI/CLBotVerbs.h"
#include "AI/CLTaskMarker.h"
#include "Game/CLSeatMotor.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLErrorBoundary.h"
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

namespace
{
	FGuid EnemySeat(UCLParticipantSeat* Seat, UCLLobbySubsystem* Lobby)
	{
		if (!Seat || !Lobby)
		{
			return FGuid();
		}
		const ECLPvpTeam Mine = Seat->GetTeam();
		for (UCLParticipantSeat* Other : Lobby->GetSeats())
		{
			if (!Other || Other == Seat)
			{
				continue;
			}
			if (Mine != ECLPvpTeam::Unassigned && Other->GetTeam() == Mine)
			{
				continue;
			}
			if (Other->GetDrivenPawn())
			{
				return Other->GetSeatId();
			}
		}
		return FGuid();
	}

	float DistXY(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
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

bool UCLBotBookManager::HasRuntime(const FGuid& SeatId) const
{
	const TSharedPtr<FRuntime>* Found = Runtimes.Find(SeatId);
	return Found && (*Found)->Stack.Num() > 0;
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
	(void)OutError;
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
	if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
	{
		Remote->CancelMotor();
		Remote->MarkReply();
	}
	else if (UCLSeatMotor* Motor = Seat->GetSeatMotor())
	{
		Motor->CancelGoto();
	}
	TSharedPtr<FRuntime>& Rt = Runtimes.FindOrAdd(Seat->GetSeatId());
	if (!Rt.IsValid())
	{
		Rt = MakeShared<FRuntime>();
	}
	Rt->bJit = false;
	Rt->JitBook.Reset();
	Rt->FallbackIndex = 0;
	return PushBook(*Rt, *Book, OutError);
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
	Book.bJit = true;
	if (UCLRemoteAgentSeatMotor* Remote = Cast<UCLRemoteAgentSeatMotor>(Seat->GetSeatMotor()))
	{
		Remote->CancelMotor();
		Remote->MarkReply();
	}
	TSharedPtr<FRuntime>& Rt = Runtimes.FindOrAdd(Seat->GetSeatId());
	if (!Rt.IsValid())
	{
		Rt = MakeShared<FRuntime>();
	}
	Rt->JitBook = MakeShared<FCLBotBook>(MoveTemp(Book));
	Rt->bJit = true;
	Rt->FallbackIndex = 0;
	return PushBook(*Rt, *Rt->JitBook, OutError);
}

bool UCLBotBookManager::BranchCatalog(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& BookName, FString& OutError)
{
	if (!Seat)
	{
		OutError = TEXT("no_seat");
		return false;
	}
	TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat->GetSeatId());
	bool bPast = true;
	if (Found && (*Found)->Stack.Num() > 0)
	{
		const FFrame& Top = (*Found)->Stack.Last();
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
	StopLeaf(*Found->Get(), Seat);
	Found->Get()->Stack.Last().Index = Found->Get()->Stack.Last().Seq->Num();
	return AppendCatalog(Seat, BookName, OutError);
}

bool UCLBotBookManager::BranchJit(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& Puml, FString& OutError)
{
	TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat ? Seat->GetSeatId() : FGuid());
	const bool bPast = !Found || !Found->IsValid() || Found->Get()->Stack.Num() == 0;
	(void)AfterId;
	(void)Offset;
	if (bPast)
	{
		return AppendJit(Seat, Puml, OutError);
	}
	return AppendJit(Seat, Puml, OutError);
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
		return ACLTaskMarker::FindById(World, FName(*Pred.Value)) != nullptr;
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
		Ctx.MarkerId = FName(**Marker);
		if (ACLTaskMarker* Mark = ACLTaskMarker::FindById(Ctx.World, Ctx.MarkerId))
		{
			Ctx.Goal = Mark->GetActorLocation();
		}
		else
		{
			UE_LOG(LogCalling, Warning, TEXT("BotBook missing marker %s"), **Marker);
			if (Stmt->Leaf.Verb == FName(TEXT("goto")))
			{
				return false;
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
	Top.Verb = CLMakeBotVerb(Stmt->Leaf.Verb);
	if (!Top.Verb.IsValid())
	{
		OutError = TEXT("unknown_verb");
		return false;
	}
	Top.bLeafStarted = true;
	Top.LeafElapsed = 0.f;
	Top.GoodEnoughHold = 0.f;
	Top.bSawGoodEnough = false;
	Top.Verb->Start(Ctx);
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
		if (ACLTaskMarker* Mark = ACLTaskMarker::FindById(Ctx.World, FName(**Marker)))
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
	TSharedPtr<FRuntime>* Found = Runtimes.Find(Seat->GetSeatId());
	if (!Found || !Found->IsValid() || Found->Get()->Stack.Num() == 0)
	{
		return false;
	}
	FRuntime& Rt = *Found->Get();
	UCLLobbySubsystem* Lobby = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	if (!Rt.FocusSeat.IsValid())
	{
		Rt.FocusSeat = EnemySeat(Seat, Lobby);
	}

	const int32 Guard = 32;
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
		ClearSeat(Seat->GetSeatId());
		return false;
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
	const TSharedPtr<FRuntime>* Found = Runtimes.Find(SeatId);
	if (!Found || !Found->IsValid() || Found->Get()->Stack.Num() == 0)
	{
		Obj->SetBoolField(TEXT("jit"), false);
		return Obj;
	}
	const FRuntime& Rt = *Found->Get();
	Obj->SetStringField(TEXT("name"), Rt.ActiveName.ToString());
	Obj->SetBoolField(TEXT("jit"), Rt.bJit);
	if (const FCLBotStmt* Stmt = CurrentStmt(Rt))
	{
		Obj->SetStringField(TEXT("nodeId"), Stmt->Id);
	}
	TArray<TSharedPtr<FJsonValue>> StackArr;
	for (const FFrame& Fr : Rt.Stack)
	{
		StackArr.Add(MakeShared<FJsonValueString>(Fr.BookName.ToString()));
	}
	Obj->SetArrayField(TEXT("stack"), StackArr);
	TArray<FString> Remain;
	CollectRemaining(Rt, Remain);
	TArray<TSharedPtr<FJsonValue>> RemArr;
	for (const FString& Id : Remain)
	{
		RemArr.Add(MakeShared<FJsonValueString>(Id));
	}
	Obj->SetArrayField(TEXT("remaining"), RemArr);
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
