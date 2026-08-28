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
	bool BranchCatalog(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& BookName, FString& OutError);
	bool BranchJit(UCLParticipantSeat* Seat, const FString& AfterId, int32 Offset, const FString& Puml, FString& OutError);
	void ClearSeat(const FGuid& SeatId);
	void NotifyRespawn(UCLParticipantSeat* Seat);

	void FillStateJson(TSharedRef<FJsonObject> Root, const FGuid& SeatId) const;
	TSharedRef<FJsonObject> MakeSeatBotJson(const FGuid& SeatId) const;

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
	};

	struct FRuntime
	{
		TArray<FFrame> Stack;
		TArray<FName> Fallbacks;
		FName OnRespawn;
		bool bJit = false;
		FName ActiveName;
		int32 FallbackIndex = 0;
		TSharedPtr<FCLBotBook> JitBook;
		FGuid FocusSeat;
	};

	void LoadCatalog();
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

	TMap<FName, TSharedPtr<FCLBotBook>> Catalog;
	TMap<FGuid, TSharedPtr<FRuntime>> Runtimes;
};
