#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/DLTypes.h"
#include "Loot/DLItemInstance.h"
#include "DLProfileSubsystem.generated.h"

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLProfileStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	int32 Kills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	int32 Deaths = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	int32 Assists = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	int32 RaidsCompleted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	int32 PvpMatchesPlayed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	float PlayTimeSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct DESTINYLIKE_API FDLLocalProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FGuid ProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	bool bIsDefault = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FDLCharacterAppearance Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FDLProfileStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	TArray<FName> CompletedMissionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	TArray<FDLItemInstance> VaultItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FGuid EquippedPrimaryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DestinyLike|Profile")
	FGuid EquippedSpecialId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDLOnProfileChanged, const FDLLocalProfile&, Profile);

/**
 * Local-only profiles. No online account. One character per profile.
 */
UCLASS()
class DESTINYLIKE_API UDLProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	void LoadAllProfiles();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	bool SaveActiveProfile();

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	bool SaveProfile(const FGuid& ProfileId);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	FDLLocalProfile CreateProfile(const FString& DisplayName);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	bool SelectProfile(const FGuid& ProfileId);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	bool SetDefaultProfile(const FGuid& ProfileId, bool bIsDefault);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Profile")
	bool LockInCharacter(const FGuid& ProfileId, const FDLCharacterAppearance& Appearance);

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Profile")
	bool HasActiveProfile() const { return ActiveProfileId.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Profile")
	FDLLocalProfile GetActiveProfile() const;

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Profile")
	TArray<FDLLocalProfile> GetAllProfiles() const { return Profiles; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Profile")
	bool ShouldAutoEnterSocial() const;

	/** C++ only — UHT forbids Blueprint-exposed pointers to USTRUCTs. */
	FDLLocalProfile* FindProfileMutable(const FGuid& ProfileId);

	const FDLLocalProfile* FindProfile(const FGuid& ProfileId) const;

	UPROPERTY(BlueprintAssignable, Category = "DestinyLike|Profile")
	FDLOnProfileChanged OnProfileChanged;

private:
	FString GetProfilesDirectory() const;
	FString GetProfileFilePath(const FGuid& ProfileId) const;
	bool WriteProfileToDisk(const FDLLocalProfile& Profile) const;
	bool ReadProfileFromDisk(const FString& FilePath, FDLLocalProfile& OutProfile) const;

	UPROPERTY()
	TArray<FDLLocalProfile> Profiles;

	UPROPERTY()
	FGuid ActiveProfileId;
};
