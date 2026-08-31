#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AI/CLBotBookTypes.h"
#include "AI/CLBotVerbs.h"
#include "Dom/JsonObject.h"
#include "CLBotBookManager.generated.h"

class UCLParticipantSeat;

UCLASS()
class CALLING_API UCLBotBookManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** @return true if a book consumed this seat's NetHz tick. */
	bool TickSeat(float DeltaSeconds, UCLParticipantSeat* Seat);
	bool HasRuntime(const FGuid& SeatId) const;

	bool AppendCatalog(UCLParticipantSeat* Seat, const FString& BookName, FString& OutError);
	bool AppendJit(UCLParticipantSeat* Seat, const FString& Puml, FString& OutError);
	bool BranchCatalog(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& BookName, const FString& Cause, FString& OutError);
	bool BranchJit(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& Puml, const FString& Cause, FString& OutError);
	void ClearSeat(const FGuid& SeatId);
	void NotifyRespawn(UCLParticipantSeat* Seat);

	/** Tick unwind + PushBook ceiling. Deeper than this cannot finish in one NetHz guard. */
	static constexpr int32 MaxBookStack = 32;
	/** FIFO books waiting after the live one. */
	static constexpr int32 MaxQueuedBooks = 4;

	void FillStateJson(TSharedRef<FJsonObject> Root, const FGuid& SeatId) const;
	TSharedRef<FJsonObject> MakeSeatBotJson(const FGuid& SeatId) const;

	/** Live goto/book not walking the pawn, unknown verb, or not local. Reports once per code/seat. */
	void NoteFollowAlert(UCLParticipantSeat* Seat, const TCHAR* Code, const FString& Detail, bool bCountExecution = true);
	void NoteMatchEvent(UCLParticipantSeat* Seat, const TCHAR* Code, const FString& Detail, bool bFailMatch);

	const FCLBotBook* FindBook(FName Name) const;
	static FString BooksDir();

protected:
	struct FFrame
	{
		FName BookName;
		const TArray<FCLBotStmt>* Seq = nullptr;
		int32 Index = 0;
		ECLBotOutcome LastOutcome = ECLBotOutcome::None;
		bool bLeafStarted = false;
		float LeafElapsed = 0.f;
		float GoodEnoughHold = 0.f;
		bool bSawGoodEnough = false;
		TUniquePtr<ICLBotVerb> Verb;
		float GotoNoStickHold = 0.f;
		FVector StillAnchor = FVector::ZeroVector;
		float PawnStillSeconds = 0.f;
	};

	struct FQueuedBook
	{
		FName CatalogName;
		TSharedPtr<FCLBotBook> Jit;
	};

	struct FRuntime
	{
		TArray<FFrame> Stack;
		TArray<FQueuedBook> Queue;
		TArray<FName> Fallbacks;
		FName OnRespawn;
		FName OnStop;
		int32 LiveOrbitIndex = -1;
		FName LiveOrbitOccupy;
		bool bJit = false;
		FName ActiveName;
		int32 FallbackIndex = 0;
		TSharedPtr<FCLBotBook> JitBook;
		TArray<TSharedPtr<FCLBotBook>> JitHold;
		FGuid FocusSeat;
		TArray<double> StrategicCancelAt;
		TArray<double> PreemptCancelAt;
		int32 ExecutionFails = 0;
		FString LastBranchCause;
		FString LastBranchNode;
		bool bReportedExecution = false;
		bool bReportedStack = false;
	};

	void LoadCatalog();
	TSharedPtr<FRuntime> EnsureRuntime(const FGuid& SeatId);
	bool IsExecuting(const FRuntime& Rt) const;
	bool CatalogAlreadyLiveOrQueued(const FRuntime& Rt, FName Name) const;
	bool NoteCancelStorm(FRuntime& Rt, bool bStrategic, FString& OutError);
	bool ParseBranchCause(const FString& Cause, ECLBotBookBranchCause& OutCause, FString& OutError) const;
	bool NoteBranchCause(UCLParticipantSeat* Seat, FRuntime& Rt, ECLBotBookBranchCause Cause, FString& OutError);
	bool EnqueueBook(FRuntime& Rt, FName CatalogName, TSharedPtr<FCLBotBook> Jit, FString& OutError);
	bool BeginNow(UCLParticipantSeat* Seat, FRuntime& Rt, const FCLBotBook& Book, FString& OutError);
	bool StartQueued(UCLParticipantSeat* Seat, FRuntime& Rt, FString& OutError);
	bool ContinueAfterExhausted(UCLParticipantSeat* Seat, FRuntime& Rt);
	bool PushBook(FRuntime& Rt, const FCLBotBook& Book, FString& OutError);
	bool AdvanceAfterLeaf(FRuntime& Rt, ECLBotOutcome Outcome);
	bool EvalPredicate(const FCLBotPredicate& Pred, UCLParticipantSeat* Seat, const FFrame* Frame, ECLBotOutcome Last) const;
	void ApplyWhile(UCLParticipantSeat* Seat, const FCLBotLeaf& Leaf) const;
	const FCLBotStmt* CurrentStmt(const FRuntime& Rt) const;
	void CollectRemaining(const FRuntime& Rt, TArray<FString>& OutIds) const;
	bool StartLeaf(FRuntime& Rt, UCLParticipantSeat* Seat, FString& OutError);
	ECLBotOutcome TickLeaf(FRuntime& Rt, float DeltaSeconds, UCLParticipantSeat* Seat);
	void StopLeaf(FRuntime& Rt, UCLParticipantSeat* Seat);
	bool TryFallback(FRuntime& Rt, FString& OutError);
	static bool HasXyzGoto(const TArray<FCLBotStmt>& Body);
	static bool CollectRefs(const TArray<FCLBotStmt>& Body, TArray<FName>& OutRefs);
	bool DetectCycles(FName From, TSet<FName>& Path, TSet<FName>& Done, FString& OutError) const;
	static bool ValidateLeaves(const TArray<FCLBotStmt>& Body, FString& OutError);

	TMap<FName, TSharedPtr<FCLBotBook>> Catalog;
	TMap<FGuid, TSharedPtr<FRuntime>> Runtimes;

	struct FBranchObs
	{
		int32 ExecutionFails = 0;
		FString LastCause;
		FString LastNode;
		FString LastBook;
		bool bReportedExecution = false;
		FString FollowAlert;
		FString LastFollowAlert;
		FString FollowAlertLive;
		TSet<FString> ReportedFollow;
	};
	TMap<FGuid, FBranchObs> BranchObs;
};
