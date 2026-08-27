#pragma once

#include "CoreMinimal.h"
#include "Core/DLTypes.h"
#include "DLLobbyTypes.generated.h"

UENUM(BlueprintType)
enum class EDLLobbyAccess : uint8
{
	Open UMETA(DisplayName = "Open"),
	Closed UMETA(DisplayName = "Closed"),
	Party UMETA(DisplayName = "Party"),
	Friends UMETA(DisplayName = "Friends"),
	Guild UMETA(DisplayName = "Guild")
};

UENUM(BlueprintType)
enum class EDLPossessionMode : uint8
{
	OwnPawn UMETA(DisplayName = "Own Pawn"),
	MindControl UMETA(DisplayName = "Mind Control"),
	Headless UMETA(DisplayName = "Headless")
};

UENUM(BlueprintType)
enum class EDLPvpTeam : uint8
{
	Unassigned UMETA(DisplayName = "Unassigned"),
	Red UMETA(DisplayName = "Red"),
	Blue UMETA(DisplayName = "Blue")
};

/** Why the hub would push a seat snapshot. Playbook decides which reasons it wants. */
UENUM(BlueprintType)
enum class EDLHubSnapshotReason : uint8
{
	Stale UMETA(DisplayName = "Stale"),
	LobbyDirty UMETA(DisplayName = "Lobby Dirty"),
	LowLookahead UMETA(DisplayName = "Low Lookahead")
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLInvoiceSeat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	FGuid SeatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	EDLPvpTeam Team = EDLPvpTeam::Unassigned;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	FString Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	FGuid DriveSeatId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	bool bHeadless = false;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLLootRealm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	FName RealmId = FName(TEXT("local"));
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLLobbyInvoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	EDLSceneId Activity = EDLSceneId::Social;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	int32 MinPlayers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	int32 MaxPlayers = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	EDLLobbyAccess Access = EDLLobbyAccess::Open;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	EDLSocialPvpMode SocialPvpMode = EDLSocialPvpMode::Optional;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	int32 RaidChamberIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	FDLLootRealm LootRealm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	TArray<FDLInvoiceSeat> Roster;

	static FDLLobbyInvoice MakeSocial(EDLLobbyAccess Access, EDLSocialPvpMode PvpMode, int32 MaxPlayers);
	static FDLLobbyInvoice MakeComposerPvp(int32 MinPlayers, int32 MaxPlayers);
	static FDLLobbyInvoice MakePvp(int32 MinPlayers, int32 MaxPlayers);
	static FDLLobbyInvoice MakeRaid(int32 ChamberIndex, int32 MinPlayers, int32 MaxPlayers);
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLLobbyGate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	float CountdownSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	float PlanStaleSeconds = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Lobby")
	float PlanLookaheadSeconds = 0.75f;
};

UCLASS()
class DESTINYLIKE_API UDLInvoiceBox : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FDLLobbyInvoice Value;
};

UCLASS()
class DESTINYLIKE_API UDLGateBox : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FDLLobbyGate Value;
};
