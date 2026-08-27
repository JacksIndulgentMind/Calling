#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/CLTypes.h"
#include "Loot/CLItemInstance.h"
#include "CLProfileSubsystem.generated.h"

USTRUCT(BlueprintType)
struct CALLING_API FCLProfileStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	int32 Kills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	int32 Deaths = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	int32 Assists = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	int32 RaidsCompleted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	int32 PvpMatchesPlayed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	float PlayTimeSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct CALLING_API FCLLocalProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	FGuid ProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	bool bIsDefault = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	FCLCharacterAppearance Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	FCLProfileStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	TArray<FName> CompletedMissionIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	TArray<FCLItemInstance> VaultItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	FGuid EquippedPrimaryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calling|Profile")
	FGuid EquippedSpecialId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCLOnProfileChanged, const FCLLocalProfile&, Profile);

/**
 * Local-only profiles. No online account. One character per profile.
 */
UCLASS()
class CALLING_API UCLProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	void LoadAllProfiles();

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	bool SaveActiveProfile();

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	bool SaveProfile(const FGuid& ProfileId);

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	FCLLocalProfile CreateProfile(const FString& DisplayName);

	/** Create + lock a default Vanguard if none is playable. Used by Boot UI skip and agent director. */
	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	bool EnsurePlayableProfile();

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	bool SelectProfile(const FGuid& ProfileId);

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	bool SetDefaultProfile(const FGuid& ProfileId, bool bIsDefault);

	UFUNCTION(BlueprintCallable, Category = "Calling|Profile")
	bool LockInCharacter(const FGuid& ProfileId, const FCLCharacterAppearance& Appearance);

	UFUNCTION(BlueprintPure, Category = "Calling|Profile")
	bool HasActiveProfile() const { return ActiveProfileId.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Calling|Profile")
	FCLLocalProfile GetActiveProfile() const;

	UFUNCTION(BlueprintPure, Category = "Calling|Profile")
	TArray<FCLLocalProfile> GetAllProfiles() const { return Profiles; }

	UFUNCTION(BlueprintPure, Category = "Calling|Profile")
	bool ShouldAutoEnterSocial() const;

	/** C++ only — UHT forbids Blueprint-exposed pointers to USTRUCTs. */
	FCLLocalProfile* FindProfileMutable(const FGuid& ProfileId);

	const FCLLocalProfile* FindProfile(const FGuid& ProfileId) const;

	UPROPERTY(BlueprintAssignable, Category = "Calling|Profile")
	FCLOnProfileChanged OnProfileChanged;

private:
	FString GetProfilesDirectory() const;
	FString GetProfileFilePath(const FGuid& ProfileId) const;
	bool WriteProfileToDisk(const FCLLocalProfile& Profile) const;
	bool ReadProfileFromDisk(const FString& FilePath, FCLLocalProfile& OutProfile) const;

	UPROPERTY()
	TArray<FCLLocalProfile> Profiles;

	UPROPERTY()
	FGuid ActiveProfileId;
};
