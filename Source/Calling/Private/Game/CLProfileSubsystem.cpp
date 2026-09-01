#include "Game/CLProfileSubsystem.h"
#include "Game/CLErrorBoundary.h"
#include "Core/CLError.h"
#include "Core/CLLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"

namespace CLProfileJson
{
	static FString ClassToString(ECLClassId Id)
	{
		switch (Id)
		{
		case ECLClassId::Pathfinder: return TEXT("pathfinder");
		case ECLClassId::Warden: return TEXT("warden");
		default: return TEXT("vanguard");
		}
	}

	static ECLClassId ClassFromString(const FString& S)
	{
		if (S.Equals(TEXT("pathfinder"), ESearchCase::IgnoreCase)) return ECLClassId::Pathfinder;
		if (S.Equals(TEXT("warden"), ESearchCase::IgnoreCase)) return ECLClassId::Warden;
		return ECLClassId::Vanguard;
	}

	static TSharedRef<FJsonObject> WeaponStatsToJson(const FCLWeaponStats& Stats)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("impact"), Stats.Impact);
		Obj->SetNumberField(TEXT("range"), Stats.Range);
		Obj->SetNumberField(TEXT("stability"), Stats.Stability);
		Obj->SetNumberField(TEXT("handling"), Stats.Handling);
		Obj->SetNumberField(TEXT("reload"), Stats.Reload);
		Obj->SetNumberField(TEXT("flinchResist"), Stats.FlinchResist);
		Obj->SetNumberField(TEXT("adsSpeed"), Stats.AdsSpeed);
		Obj->SetNumberField(TEXT("mobilityBonus"), Stats.MobilityBonus);
		Obj->SetNumberField(TEXT("magazine"), Stats.Magazine);
		Obj->SetNumberField(TEXT("rpm"), Stats.Rpm);
		Obj->SetNumberField(TEXT("massKg"), Stats.MassKg);
		Obj->SetNumberField(TEXT("barrelLengthCm"), Stats.BarrelLengthCm);
		Obj->SetNumberField(TEXT("ammoGrains"), Stats.AmmoGrains);
		Obj->SetNumberField(TEXT("muzzleVelocityMps"), Stats.MuzzleVelocityMps);
		Obj->SetNumberField(TEXT("grip"), Stats.Grip);
		Obj->SetNumberField(TEXT("compensator"), Stats.Compensator);
		Obj->SetNumberField(TEXT("drawSeconds"), Stats.DrawSeconds);
		Obj->SetNumberField(TEXT("stowSeconds"), Stats.StowSeconds);
		return Obj;
	}

	static void WeaponStatsFromJson(const TSharedPtr<FJsonObject>& Obj, FCLWeaponStats& Stats)
	{
		if (!Obj.IsValid()) return;
		Stats.Impact = Obj->GetNumberField(TEXT("impact"));
		Stats.Range = Obj->GetNumberField(TEXT("range"));
		Stats.Stability = Obj->GetNumberField(TEXT("stability"));
		Stats.Handling = Obj->GetNumberField(TEXT("handling"));
		Stats.Reload = Obj->GetNumberField(TEXT("reload"));
		Stats.FlinchResist = Obj->GetNumberField(TEXT("flinchResist"));
		Stats.AdsSpeed = Obj->GetNumberField(TEXT("adsSpeed"));
		Stats.MobilityBonus = Obj->GetNumberField(TEXT("mobilityBonus"));
		Stats.Magazine = static_cast<int32>(Obj->GetNumberField(TEXT("magazine")));
		Stats.Rpm = Obj->GetNumberField(TEXT("rpm"));
		if (Obj->HasField(TEXT("massKg"))) { Stats.MassKg = Obj->GetNumberField(TEXT("massKg")); }
		if (Obj->HasField(TEXT("barrelLengthCm"))) { Stats.BarrelLengthCm = Obj->GetNumberField(TEXT("barrelLengthCm")); }
		if (Obj->HasField(TEXT("ammoGrains"))) { Stats.AmmoGrains = Obj->GetNumberField(TEXT("ammoGrains")); }
		if (Obj->HasField(TEXT("muzzleVelocityMps"))) { Stats.MuzzleVelocityMps = Obj->GetNumberField(TEXT("muzzleVelocityMps")); }
		if (Obj->HasField(TEXT("grip"))) { Stats.Grip = Obj->GetNumberField(TEXT("grip")); }
		if (Obj->HasField(TEXT("compensator"))) { Stats.Compensator = Obj->GetNumberField(TEXT("compensator")); }
		if (Obj->HasField(TEXT("drawSeconds"))) { Stats.DrawSeconds = Obj->GetNumberField(TEXT("drawSeconds")); }
		if (Obj->HasField(TEXT("stowSeconds"))) { Stats.StowSeconds = Obj->GetNumberField(TEXT("stowSeconds")); }
	}

	static TSharedRef<FJsonObject> ItemToJson(const FCLItemInstance& Item)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("instanceId"), Item.InstanceId.ToString(EGuidFormats::DigitsWithHyphens));
		Obj->SetStringField(TEXT("kind"), Item.Kind == ECLItemKind::Armor ? TEXT("armor") : TEXT("weapon"));
		Obj->SetNumberField(TEXT("rarity"), static_cast<int32>(Item.Rarity));
		Obj->SetStringField(TEXT("definitionId"), Item.DefinitionId.ToString());
		Obj->SetStringField(TEXT("displayName"), Item.DisplayName);
		Obj->SetNumberField(TEXT("weaponSlot"), static_cast<int32>(Item.Weapon.Slot));
		Obj->SetNumberField(TEXT("armorPiece"), static_cast<int32>(Item.Armor.Piece));
		Obj->SetObjectField(TEXT("baseStats"), WeaponStatsToJson(Item.BaseStats));
		Obj->SetObjectField(TEXT("finalStats"), WeaponStatsToJson(Item.FinalStats));
		Obj->SetStringField(TEXT("sourceTableId"), Item.SourceTableId);
		Obj->SetStringField(TEXT("sightId"), Item.SightId.ToString());
		Obj->SetStringField(TEXT("earnedAt"), Item.EarnedAt.ToIso8601());

		TArray<TSharedPtr<FJsonValue>> Mods;
		for (const FCLModifierRoll& Mod : Item.Modifiers)
		{
			TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
			M->SetStringField(TEXT("modifierId"), Mod.ModifierId.ToString());
			M->SetStringField(TEXT("displayName"), Mod.DisplayName);
			M->SetStringField(TEXT("behaviorId"), Mod.BehaviorId.ToString());
			M->SetNumberField(TEXT("behaviorScale"), Mod.BehaviorScale);
			M->SetObjectField(TEXT("statDelta"), WeaponStatsToJson(Mod.StatDelta));
			Mods.Add(MakeShared<FJsonValueObject>(M));
		}
		Obj->SetArrayField(TEXT("modifiers"), Mods);
		return Obj;
	}

	static bool ItemFromJson(const TSharedPtr<FJsonObject>& Obj, FCLItemInstance& Item)
	{
		if (!Obj.IsValid()) return false;
		FGuid::Parse(Obj->GetStringField(TEXT("instanceId")), Item.InstanceId);
		Item.Kind = Obj->GetStringField(TEXT("kind")).Equals(TEXT("armor"), ESearchCase::IgnoreCase)
			? ECLItemKind::Armor : ECLItemKind::Weapon;
		Item.Rarity = static_cast<ECLItemRarity>(static_cast<int32>(Obj->GetNumberField(TEXT("rarity"))));
		Item.DefinitionId = FName(*Obj->GetStringField(TEXT("definitionId")));
		Item.DisplayName = Obj->GetStringField(TEXT("displayName"));
		Item.Weapon.Slot = static_cast<ECLWeaponSlot>(static_cast<int32>(Obj->GetNumberField(TEXT("weaponSlot"))));
		Item.Armor.Piece = static_cast<ECLArmorPiece>(static_cast<int32>(Obj->GetNumberField(TEXT("armorPiece"))));
		WeaponStatsFromJson(Obj->GetObjectField(TEXT("baseStats")), Item.BaseStats);
		WeaponStatsFromJson(Obj->GetObjectField(TEXT("finalStats")), Item.FinalStats);
		Item.SourceTableId = Obj->GetStringField(TEXT("sourceTableId"));
		if (Obj->HasField(TEXT("sightId")))
		{
			Item.SightId = FName(*Obj->GetStringField(TEXT("sightId")));
		}
		FDateTime::ParseIso8601(*Obj->GetStringField(TEXT("earnedAt")), Item.EarnedAt);

		Item.Modifiers.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Mods = nullptr;
		if (Obj->TryGetArrayField(TEXT("modifiers"), Mods) && Mods)
		{
			for (const TSharedPtr<FJsonValue>& V : *Mods)
			{
				const TSharedPtr<FJsonObject> M = V->AsObject();
				if (!M.IsValid()) continue;
				FCLModifierRoll Roll;
				Roll.ModifierId = FName(*M->GetStringField(TEXT("modifierId")));
				Roll.DisplayName = M->GetStringField(TEXT("displayName"));
				Roll.BehaviorId = FName(*M->GetStringField(TEXT("behaviorId")));
				Roll.BehaviorScale = M->GetNumberField(TEXT("behaviorScale"));
				WeaponStatsFromJson(M->GetObjectField(TEXT("statDelta")), Roll.StatDelta);
				Item.Modifiers.Add(Roll);
			}
		}
		return true;
	}

	static TSharedRef<FJsonObject> ProfileToJson(const FCLLocalProfile& P)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("profileId"), P.ProfileId.ToString(EGuidFormats::DigitsWithHyphens));
		Obj->SetStringField(TEXT("displayName"), P.DisplayName);
		Obj->SetBoolField(TEXT("isDefault"), P.bIsDefault);

		TSharedRef<FJsonObject> CharObj = MakeShared<FJsonObject>();
		CharObj->SetStringField(TEXT("classId"), ClassToString(P.Character.ClassId));
		CharObj->SetNumberField(TEXT("sex"), static_cast<int32>(P.Character.Sex));
		CharObj->SetStringField(TEXT("lookId"), P.Character.LookId.ToString());
		CharObj->SetStringField(TEXT("characterName"), P.Character.CharacterName);
		CharObj->SetBoolField(TEXT("lockedIn"), P.Character.bLockedIn);
		Obj->SetObjectField(TEXT("character"), CharObj);

		TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
		Stats->SetNumberField(TEXT("kills"), P.Stats.Kills);
		Stats->SetNumberField(TEXT("deaths"), P.Stats.Deaths);
		Stats->SetNumberField(TEXT("assists"), P.Stats.Assists);
		Stats->SetNumberField(TEXT("raidsCompleted"), P.Stats.RaidsCompleted);
		Stats->SetNumberField(TEXT("pvpMatchesPlayed"), P.Stats.PvpMatchesPlayed);
		Stats->SetNumberField(TEXT("playTimeSeconds"), P.Stats.PlayTimeSeconds);
		Obj->SetObjectField(TEXT("stats"), Stats);

		TArray<TSharedPtr<FJsonValue>> Missions;
		for (const FName& Mid : P.CompletedMissionIds)
		{
			Missions.Add(MakeShared<FJsonValueString>(Mid.ToString()));
		}
		Obj->SetArrayField(TEXT("completedMissionIds"), Missions);

		TArray<TSharedPtr<FJsonValue>> Vault;
		for (const FCLItemInstance& Item : P.VaultItems)
		{
			Vault.Add(MakeShared<FJsonValueObject>(ItemToJson(Item)));
		}
		Obj->SetArrayField(TEXT("vaultItems"), Vault);
		Obj->SetStringField(TEXT("equippedPrimaryId"), P.EquippedPrimaryId.ToString(EGuidFormats::DigitsWithHyphens));
		Obj->SetStringField(TEXT("equippedSpecialId"), P.EquippedSpecialId.ToString(EGuidFormats::DigitsWithHyphens));

		TSharedRef<FJsonObject> Social = MakeShared<FJsonObject>();
		Social->SetStringField(TEXT("kind"), FCLSocialDefault::KindToString(P.SocialDefault.Kind));
		Social->SetStringField(TEXT("joinHost"), P.SocialDefault.JoinHost.IsEmpty() ? TEXT("127.0.0.1") : P.SocialDefault.JoinHost);
		Social->SetNumberField(TEXT("joinPort"), P.SocialDefault.JoinPort > 0 ? P.SocialDefault.JoinPort : 7777);
		Social->SetStringField(TEXT("joinFallback"), FCLSocialDefault::FallbackToString(P.SocialDefault.JoinFallback));
		Obj->SetObjectField(TEXT("socialDefault"), Social);
		return Obj;
	}

	static bool ProfileFromJson(const TSharedPtr<FJsonObject>& Obj, FCLLocalProfile& P)
	{
		if (!Obj.IsValid()) return false;
		FGuid::Parse(Obj->GetStringField(TEXT("profileId")), P.ProfileId);
		P.DisplayName = Obj->GetStringField(TEXT("displayName"));
		P.bIsDefault = Obj->GetBoolField(TEXT("isDefault"));

		if (const TSharedPtr<FJsonObject> CharObj = Obj->GetObjectField(TEXT("character")))
		{
			P.Character.ClassId = ClassFromString(CharObj->GetStringField(TEXT("classId")));
			P.Character.Sex = static_cast<ECLCharacterSex>(static_cast<int32>(CharObj->GetNumberField(TEXT("sex"))));
			P.Character.LookId = FName(*CharObj->GetStringField(TEXT("lookId")));
			P.Character.CharacterName = CharObj->GetStringField(TEXT("characterName"));
			P.Character.bLockedIn = CharObj->GetBoolField(TEXT("lockedIn"));
		}

		if (const TSharedPtr<FJsonObject> Stats = Obj->GetObjectField(TEXT("stats")))
		{
			P.Stats.Kills = static_cast<int32>(Stats->GetNumberField(TEXT("kills")));
			P.Stats.Deaths = static_cast<int32>(Stats->GetNumberField(TEXT("deaths")));
			P.Stats.Assists = static_cast<int32>(Stats->GetNumberField(TEXT("assists")));
			P.Stats.RaidsCompleted = static_cast<int32>(Stats->GetNumberField(TEXT("raidsCompleted")));
			P.Stats.PvpMatchesPlayed = static_cast<int32>(Stats->GetNumberField(TEXT("pvpMatchesPlayed")));
			P.Stats.PlayTimeSeconds = Stats->GetNumberField(TEXT("playTimeSeconds"));
		}

		P.CompletedMissionIds.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Missions = nullptr;
		if (Obj->TryGetArrayField(TEXT("completedMissionIds"), Missions) && Missions)
		{
			for (const TSharedPtr<FJsonValue>& V : *Missions)
			{
				P.CompletedMissionIds.Add(FName(*V->AsString()));
			}
		}

		P.VaultItems.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Vault = nullptr;
		if (Obj->TryGetArrayField(TEXT("vaultItems"), Vault) && Vault)
		{
			for (const TSharedPtr<FJsonValue>& V : *Vault)
			{
				FCLItemInstance Item;
				if (ItemFromJson(V->AsObject(), Item))
				{
					P.VaultItems.Add(Item);
				}
			}
		}

		FGuid::Parse(Obj->GetStringField(TEXT("equippedPrimaryId")), P.EquippedPrimaryId);
		FGuid::Parse(Obj->GetStringField(TEXT("equippedSpecialId")), P.EquippedSpecialId);

		if (const TSharedPtr<FJsonObject> Social = Obj->HasField(TEXT("socialDefault")) ? Obj->GetObjectField(TEXT("socialDefault")) : nullptr)
		{
			P.SocialDefault.Kind = FCLSocialDefault::KindFromString(Social->GetStringField(TEXT("kind")));
			if (Social->HasField(TEXT("joinHost")))
			{
				P.SocialDefault.JoinHost = Social->GetStringField(TEXT("joinHost"));
			}
			if (Social->HasField(TEXT("joinPort")))
			{
				P.SocialDefault.JoinPort = static_cast<int32>(Social->GetNumberField(TEXT("joinPort")));
			}
			if (Social->HasField(TEXT("joinFallback")))
			{
				P.SocialDefault.JoinFallback = FCLSocialDefault::FallbackFromString(Social->GetStringField(TEXT("joinFallback")));
			}
		}
		if (P.SocialDefault.JoinHost.IsEmpty())
		{
			P.SocialDefault.JoinHost = TEXT("127.0.0.1");
		}
		if (P.SocialDefault.JoinPort <= 0)
		{
			P.SocialDefault.JoinPort = 7777;
		}
		return true;
	}
}

void UCLProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

FString UCLProfileSubsystem::GetProfilesDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calling"), TEXT("Profiles"));
}

FString UCLProfileSubsystem::GetProfileFilePath(const FGuid& ProfileId) const
{
	return FPaths::Combine(GetProfilesDirectory(), ProfileId.ToString(EGuidFormats::DigitsWithHyphens) + TEXT(".json"));
}

void UCLProfileSubsystem::LoadAllProfiles()
{
	Profiles.Reset();
	ActiveProfileId.Invalidate();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	const FString Dir = GetProfilesDirectory();
	PlatformFile.CreateDirectoryTree(*Dir);

	TArray<FString> Files;
	PlatformFile.FindFiles(Files, *Dir, TEXT(".json"));
	for (const FString& File : Files)
	{
		FCLLocalProfile Profile;
		if (ReadProfileFromDisk(File, Profile))
		{
			Profiles.Add(Profile);
		}
		else
		{
			UE_LOG(LogCalling, Error, TEXT("Corrupt profile skipped: %s"), *File);
			UCLErrorBoundary::ReportStatic(this, FCLError::Make(
				ECLErrorKind::NonDeterministic,
				TEXT("profile_read"),
				FString::Printf(TEXT("Could not read %s"), *File)));
		}
	}

	for (const FCLLocalProfile& P : Profiles)
	{
		if (P.bIsDefault)
		{
			ActiveProfileId = P.ProfileId;
			break;
		}
	}

	if (!ActiveProfileId.IsValid() && Profiles.Num() == 1)
	{
		ActiveProfileId = Profiles[0].ProfileId;
	}
}

bool UCLProfileSubsystem::WriteProfileToDisk(const FCLLocalProfile& Profile) const
{
	const TSharedRef<FJsonObject> Obj = CLProfileJson::ProfileToJson(Profile);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	if (!FJsonSerializer::Serialize(Obj, Writer))
	{
		UE_LOG(LogCalling, Error, TEXT("Profile serialize failed"));
		UCLErrorBoundary::ReportStatic(const_cast<UCLProfileSubsystem*>(this), FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("profile_write"),
			TEXT("Could not serialize profile")));
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*GetProfilesDirectory());
	if (!FFileHelper::SaveStringToFile(Out, *GetProfileFilePath(Profile.ProfileId)))
	{
		UE_LOG(LogCalling, Error, TEXT("Profile save failed"));
		UCLErrorBoundary::ReportStatic(const_cast<UCLProfileSubsystem*>(this), FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("profile_write"),
			TEXT("Could not write profile file")));
		return false;
	}
	return true;
}

bool UCLProfileSubsystem::ReadProfileFromDisk(const FString& FilePath, FCLLocalProfile& OutProfile) const
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		return false;
	}
	return CLProfileJson::ProfileFromJson(Obj, OutProfile);
}

bool UCLProfileSubsystem::SaveActiveProfile()
{
	if (!ActiveProfileId.IsValid())
	{
		return false;
	}
	return SaveProfile(ActiveProfileId);
}

bool UCLProfileSubsystem::SaveProfile(const FGuid& ProfileId)
{
	if (const FCLLocalProfile* Found = FindProfile(ProfileId))
	{
		return WriteProfileToDisk(*Found);
	}
	return false;
}

FCLLocalProfile UCLProfileSubsystem::CreateProfile(const FString& DisplayName)
{
	FCLLocalProfile Profile;
	Profile.ProfileId = FGuid::NewGuid();
	Profile.DisplayName = DisplayName.IsEmpty() ? TEXT("Player") : DisplayName;
	Profile.bIsDefault = Profiles.Num() == 0;
	Profiles.Add(Profile);
	WriteProfileToDisk(Profile);
	ActiveProfileId = Profile.ProfileId;
	OnProfileChanged.Broadcast(Profile);
	return Profile;
}

bool UCLProfileSubsystem::EnsurePlayableProfile()
{
	LoadAllProfiles();
	if (ShouldAutoEnterSocial())
	{
		if (!HasActiveProfile())
		{
			const TArray<FCLLocalProfile> All = GetAllProfiles();
			if (All.Num() > 0)
			{
				SelectProfile(All[0].ProfileId);
			}
		}
		return true;
	}

	FCLLocalProfile Profile = CreateProfile(TEXT("Player"));
	FCLCharacterAppearance Appearance;
	Appearance.ClassId = ECLClassId::Vanguard;
	Appearance.Sex = ECLCharacterSex::Other;
	Appearance.LookId = FName(TEXT("look_2"));
	Appearance.CharacterName = TEXT("Player");
	if (!LockInCharacter(Profile.ProfileId, Appearance))
	{
		return false;
	}
	SetDefaultProfile(Profile.ProfileId, true);
	SelectProfile(Profile.ProfileId);
	return ShouldAutoEnterSocial();
}

bool UCLProfileSubsystem::SelectProfile(const FGuid& ProfileId)
{
	if (!FindProfile(ProfileId))
	{
		return false;
	}
	ActiveProfileId = ProfileId;
	OnProfileChanged.Broadcast(GetActiveProfile());
	return true;
}

bool UCLProfileSubsystem::SetDefaultProfile(const FGuid& ProfileId, bool bIsDefault)
{
	bool bFound = false;
	for (FCLLocalProfile& P : Profiles)
	{
		if (P.ProfileId == ProfileId)
		{
			P.bIsDefault = bIsDefault;
			bFound = true;
		}
		else if (bIsDefault)
		{
			P.bIsDefault = false;
		}
		WriteProfileToDisk(P);
	}
	if (bFound)
	{
		OnProfileChanged.Broadcast(GetActiveProfile());
	}
	return bFound;
}

bool UCLProfileSubsystem::LockInCharacter(const FGuid& ProfileId, const FCLCharacterAppearance& Appearance)
{
	FCLLocalProfile* Profile = FindProfileMutable(ProfileId);
	if (!Profile)
	{
		return false;
	}

	Profile->Character = Appearance;
	Profile->Character.bLockedIn = true;
	WriteProfileToDisk(*Profile);
	OnProfileChanged.Broadcast(*Profile);
	return true;
}

FCLLocalProfile UCLProfileSubsystem::GetActiveProfile() const
{
	if (const FCLLocalProfile* Found = FindProfile(ActiveProfileId))
	{
		return *Found;
	}
	return FCLLocalProfile();
}

bool UCLProfileSubsystem::ShouldAutoEnterSocial() const
{
	if (Profiles.Num() == 1 && Profiles[0].Character.bLockedIn)
	{
		return true;
	}
	for (const FCLLocalProfile& P : Profiles)
	{
		if (P.bIsDefault && P.Character.bLockedIn)
		{
			return true;
		}
	}
	return false;
}

bool UCLProfileSubsystem::SetSocialDefault(const FCLSocialDefault& Default)
{
	if (!ActiveProfileId.IsValid())
	{
		EnsurePlayableProfile();
	}
	FCLLocalProfile* Profile = FindProfileMutable(ActiveProfileId);
	if (!Profile)
	{
		return false;
	}
	Profile->SocialDefault = Default;
	if (Profile->SocialDefault.JoinHost.IsEmpty())
	{
		Profile->SocialDefault.JoinHost = TEXT("127.0.0.1");
	}
	if (Profile->SocialDefault.JoinPort <= 0)
	{
		Profile->SocialDefault.JoinPort = 7777;
	}
	WriteProfileToDisk(*Profile);
	OnProfileChanged.Broadcast(*Profile);
	return true;
}

FCLSocialDefault UCLProfileSubsystem::GetSocialDefault() const
{
	if (const FCLLocalProfile* Found = FindProfile(ActiveProfileId))
	{
		return Found->SocialDefault;
	}
	return FCLSocialDefault();
}

FCLLocalProfile* UCLProfileSubsystem::FindProfileMutable(const FGuid& ProfileId)
{
	for (FCLLocalProfile& P : Profiles)
	{
		if (P.ProfileId == ProfileId)
		{
			return &P;
		}
	}
	return nullptr;
}

const FCLLocalProfile* UCLProfileSubsystem::FindProfile(const FGuid& ProfileId) const
{
	for (const FCLLocalProfile& P : Profiles)
	{
		if (P.ProfileId == ProfileId)
		{
			return &P;
		}
	}
	return nullptr;
}
