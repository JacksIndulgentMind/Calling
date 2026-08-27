#pragma once

#include "CoreMinimal.h"
#include "DLTypes.generated.h"

UENUM(BlueprintType)
enum class EDLSceneId : uint8
{
	Boot UMETA(DisplayName = "Boot"),
	Social UMETA(DisplayName = "Social"),
	Composer UMETA(DisplayName = "Composer"),
	Pvp UMETA(DisplayName = "PvP"),
	Raid UMETA(DisplayName = "Raid"),
	Practice UMETA(DisplayName = "Practice")
};

UENUM(BlueprintType)
enum class EDLActivityPhase : uint8
{
	Lobby UMETA(DisplayName = "Lobby"),
	Loading UMETA(DisplayName = "Loading"),
	InProgress UMETA(DisplayName = "In Progress"),
	Results UMETA(DisplayName = "Results"),
	Returning UMETA(DisplayName = "Returning")
};

UENUM(BlueprintType)
enum class EDLSocialPvpMode : uint8
{
	Optional UMETA(DisplayName = "PvP Optional"),
	Forced UMETA(DisplayName = "PvP Forced")
};

UENUM(BlueprintType)
enum class EDLWeaponSlot : uint8
{
	Primary UMETA(DisplayName = "Primary"),
	Special UMETA(DisplayName = "Special")
};

UENUM(BlueprintType)
enum class EDLItemKind : uint8
{
	Weapon UMETA(DisplayName = "Weapon"),
	Armor UMETA(DisplayName = "Armor")
};

UENUM(BlueprintType)
enum class EDLItemRarity : uint8
{
	Common UMETA(DisplayName = "Common"),
	Uncommon UMETA(DisplayName = "Uncommon"),
	Rare UMETA(DisplayName = "Rare"),
	Epic UMETA(DisplayName = "Epic"),
	Legendary UMETA(DisplayName = "Legendary"),
	Exotic UMETA(DisplayName = "Exotic")
};

UENUM(BlueprintType)
enum class EDLArmorPiece : uint8
{
	Helm UMETA(DisplayName = "Helm"),
	Arms UMETA(DisplayName = "Arms"),
	Chest UMETA(DisplayName = "Chest"),
	Legs UMETA(DisplayName = "Legs")
};

UENUM(BlueprintType)
enum class EDLClassId : uint8
{
	Vanguard UMETA(DisplayName = "Vanguard"),
	Pathfinder UMETA(DisplayName = "Pathfinder"),
	Warden UMETA(DisplayName = "Warden")
};

UENUM(BlueprintType)
enum class EDLCharacterSex : uint8
{
	Male UMETA(DisplayName = "Male"),
	Female UMETA(DisplayName = "Female"),
	Other UMETA(DisplayName = "Other")
};

UENUM(BlueprintType)
enum class EDLNavPersonality : uint8
{
	Wanderer UMETA(DisplayName = "Wanderer"),
	CoverCycler UMETA(DisplayName = "Cover Cycler"),
	Flanker UMETA(DisplayName = "Flanker"),
	HoldGround UMETA(DisplayName = "Hold Ground"),
	AggressivePush UMETA(DisplayName = "Aggressive Push"),
	CircleConfused UMETA(DisplayName = "Circle Confused")
};

UENUM(BlueprintType)
enum class EDLEngagementPersonality : uint8
{
	Pusher UMETA(DisplayName = "Pusher"),
	Flanker UMETA(DisplayName = "Flanker"),
	Sniper UMETA(DisplayName = "Sniper"),
	Grenadier UMETA(DisplayName = "Grenadier"),
	Ambusher UMETA(DisplayName = "Ambusher"),
	CeilingShooter UMETA(DisplayName = "Ceiling Shooter"),
	WeaponThrower UMETA(DisplayName = "Weapon Thrower"),
	IdleTroll UMETA(DisplayName = "Idle Troll")
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLCharacterAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	EDLClassId ClassId = EDLClassId::Vanguard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	EDLCharacterSex Sex = EDLCharacterSex::Other;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FName LookId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	bool bLockedIn = false;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLLobbyListing
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	FString SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	FString HostDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	EDLSceneId Activity = EDLSceneId::Social;

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	int32 MaxPlayers = 8;

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	EDLSocialPvpMode SocialPvpMode = EDLSocialPvpMode::Optional;

	UPROPERTY(BlueprintReadOnly, Category = "DestinyLike|Session")
	FString MapName;
};
