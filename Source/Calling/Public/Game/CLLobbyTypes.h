#pragma once

#include "CoreMinimal.h"
#include "Core/CLTypes.h"
#include "CLLobbyTypes.generated.h"

UENUM(BlueprintType)
enum class ECLLobbyAccess : uint8
{
	Open UMETA(DisplayName = "Open"),
	Closed UMETA(DisplayName = "Closed"),
	Party UMETA(DisplayName = "Party"),
	Friends UMETA(DisplayName = "Friends"),
	Guild UMETA(DisplayName = "Guild")
};

UENUM(BlueprintType)
enum class ECLSocialDefaultKind : uint8
{
	Private UMETA(DisplayName = "Private"),
	Public UMETA(DisplayName = "Public"),
	Friends UMETA(DisplayName = "Friends"),
	Party UMETA(DisplayName = "Party"),
	Join UMETA(DisplayName = "Join")
};

UENUM(BlueprintType)
enum class ECLSocialJoinFallback : uint8
{
	Private UMETA(DisplayName = "Private"),
	Public UMETA(DisplayName = "Public")
};

USTRUCT(BlueprintType)
struct CALLING_API FCLSocialDefault
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Social")
	ECLSocialDefaultKind Kind = ECLSocialDefaultKind::Private;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Social")
	FString JoinHost = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Social")
	int32 JoinPort = 7777;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Social")
	ECLSocialJoinFallback JoinFallback = ECLSocialJoinFallback::Private;

	static ECLSocialDefaultKind KindFromString(const FString& S);
	static FString KindToString(ECLSocialDefaultKind Kind);
	static ECLSocialJoinFallback FallbackFromString(const FString& S);
	static FString FallbackToString(ECLSocialJoinFallback Fallback);
	static ECLLobbyAccess AccessForKind(ECLSocialDefaultKind Kind);
};

UENUM(BlueprintType)
enum class ECLPossessionMode : uint8
{
	OwnPawn UMETA(DisplayName = "Own Pawn"),
	MindControl UMETA(DisplayName = "Mind Control"),
	Headless UMETA(DisplayName = "Headless")
};

UENUM(BlueprintType)
enum class ECLPvpTeam : uint8
{
	Unassigned UMETA(DisplayName = "Unassigned"),
	Red UMETA(DisplayName = "Red"),
	Blue UMETA(DisplayName = "Blue")
};

/** Why the hub would push a seat snapshot. SeatMotor decides which reasons it wants. */
UENUM(BlueprintType)
enum class ECLHubSnapshotReason : uint8
{
	Stale UMETA(DisplayName = "Stale"),
	LobbyDirty UMETA(DisplayName = "Lobby Dirty"),
	LowLookahead UMETA(DisplayName = "Low Lookahead")
};

USTRUCT(BlueprintType)
struct CALLING_API FCLLobbySeatSnap
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SeatId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	ECLPvpTeam Team = ECLPvpTeam::Unassigned;

	UPROPERTY()
	bool bReady = false;

	UPROPERTY()
	bool bHost = false;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLInvoiceSeat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FGuid SeatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	ECLPvpTeam Team = ECLPvpTeam::Unassigned;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FGuid DriveSeatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	bool bHeadless = false;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLLootRealm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FName RealmId = FName(TEXT("local"));
};

USTRUCT(BlueprintType)
struct CALLING_API FCLLobbyInvoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	ECLSceneId Activity = ECLSceneId::Social;

	/** JSON game mode id (e.g. shrine_clash). Not an Unreal AGameMode class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FName GameModeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	int32 MinPlayers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	int32 MaxPlayers = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	ECLLobbyAccess Access = ECLLobbyAccess::Open;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	ECLSocialPvpMode SocialPvpMode = ECLSocialPvpMode::Optional;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	int32 RaidChamberIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	FCLLootRealm LootRealm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	TArray<FCLInvoiceSeat> Roster;

	static FCLLobbyInvoice MakeSocial(ECLLobbyAccess Access, ECLSocialPvpMode PvpMode, int32 MaxPlayers);
	static FCLLobbyInvoice MakeComposerPvp(int32 MinPlayers, int32 MaxPlayers);
	static FCLLobbyInvoice MakePvp(int32 MinPlayers, int32 MaxPlayers);
	static FCLLobbyInvoice MakeRaid(int32 ChamberIndex, int32 MinPlayers, int32 MaxPlayers);
};

USTRUCT(BlueprintType)
struct CALLING_API FCLLobbyGate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	float CountdownSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	float PlanStaleSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Lobby")
	float PlanLookaheadSeconds = 0.75f;
};

UCLASS()
class CALLING_API UCLInvoiceBox : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FCLLobbyInvoice Value;
};

UCLASS()
class CALLING_API UCLGateBox : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FCLLobbyGate Value;
};
