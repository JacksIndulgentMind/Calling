#include "Game/CLInputBindSubsystem.h"
#include "Core/CLLog.h"
#include "Core/CLError.h"
#include "Game/CLErrorBoundary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"

namespace
{
	TSharedRef<FJsonObject> ChordToJson(const FCLKeyChord& Chord)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (Chord.IsSet())
		{
			Obj->SetStringField(TEXT("key"), Chord.Key.ToString());
			Obj->SetBoolField(TEXT("alt"), Chord.bAlt);
		}
		return Obj;
	}

	FCLKeyChord ChordFromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FCLKeyChord Chord;
		if (!Obj.IsValid())
		{
			return Chord;
		}
		FString KeyName;
		if (Obj->TryGetStringField(TEXT("key"), KeyName) && !KeyName.IsEmpty())
		{
			Chord.Key = FKey(*KeyName);
			Chord.bAlt = Obj->GetBoolField(TEXT("alt"));
		}
		if (CLInput::IsAltKey(Chord.Key))
		{
			return FCLKeyChord();
		}
		return Chord;
	}

	FCLKeyChord MakeChord(const FKey& Key, bool bAlt = false)
	{
		FCLKeyChord Chord;
		Chord.Key = Key;
		Chord.bAlt = bAlt;
		return Chord;
	}

	FCLKeyChord& ChordSlot(FCLActionBinds& Pair, ECLBindColumn Column)
	{
		switch (Column)
		{
		case ECLBindColumn::Primary: return Pair.Primary;
		case ECLBindColumn::Secondary: return Pair.Secondary;
		default: return Pair.Gamepad;
		}
	}

	const FCLKeyChord& ChordSlot(const FCLActionBinds& Pair, ECLBindColumn Column)
	{
		switch (Column)
		{
		case ECLBindColumn::Primary: return Pair.Primary;
		case ECLBindColumn::Secondary: return Pair.Secondary;
		default: return Pair.Gamepad;
		}
	}
}

TMap<ECLBindableAction, FCLActionBinds> UCLInputBindSubsystem::MakeDefaults()
{
	TMap<ECLBindableAction, FCLActionBinds> Out;
	auto Set = [&Out](ECLBindableAction Action, const FKey& Primary, const FKey& Secondary = EKeys::Invalid, const FKey& Gamepad = EKeys::Invalid)
	{
		FCLActionBinds Binds;
		Binds.Primary = MakeChord(Primary);
		if (Secondary.IsValid())
		{
			Binds.Secondary = MakeChord(Secondary);
		}
		if (Gamepad.IsValid())
		{
			Binds.Gamepad = MakeChord(Gamepad);
		}
		Out.Add(Action, Binds);
	};

	Set(ECLBindableAction::Fire, EKeys::LeftMouseButton, EKeys::Invalid, EKeys::Gamepad_RightTrigger);
	Set(ECLBindableAction::ADS, EKeys::RightMouseButton, EKeys::Invalid, EKeys::Gamepad_LeftTrigger);
	Set(ECLBindableAction::Reload, EKeys::R, EKeys::Invalid, EKeys::Gamepad_FaceButton_Left);
	Set(ECLBindableAction::Swap, EKeys::Q, EKeys::Invalid, EKeys::Gamepad_FaceButton_Top);
	Set(ECLBindableAction::WeaponPrimary, EKeys::One);
	Set(ECLBindableAction::WeaponSpecial, EKeys::Two);
	Set(ECLBindableAction::Jump, EKeys::SpaceBar, EKeys::Invalid, EKeys::Gamepad_FaceButton_Bottom);
	Set(ECLBindableAction::Sprint, EKeys::LeftShift, EKeys::Invalid, EKeys::Gamepad_LeftThumbstick);
	Set(ECLBindableAction::Crouch, EKeys::C, EKeys::Invalid, EKeys::Gamepad_FaceButton_Right);
	Set(ECLBindableAction::Slide, EKeys::LeftControl);
	Set(ECLBindableAction::AirDive, EKeys::Z, EKeys::Invalid, EKeys::Gamepad_DPad_Down);
	Set(ECLBindableAction::Dodge, EKeys::X);
	Set(ECLBindableAction::Grenade, EKeys::B, EKeys::Invalid, EKeys::Gamepad_LeftShoulder);
	Set(ECLBindableAction::Melee, EKeys::F, EKeys::Invalid, EKeys::Gamepad_RightShoulder);
	Set(ECLBindableAction::Dash, EKeys::V, EKeys::Invalid, EKeys::Gamepad_DPad_Up);
	Set(ECLBindableAction::Shield, EKeys::G, EKeys::Invalid, EKeys::Gamepad_DPad_Left);
	Set(ECLBindableAction::Evasion, EKeys::E, EKeys::Invalid, EKeys::Gamepad_DPad_Right);
	Set(ECLBindableAction::Super, EKeys::T);
	return Out;
}

void UCLInputBindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Binds = MakeDefaults();
	FString QuirkyText;
	const FString QuirkyPath = GetQuirkyPath();
	if (!FFileHelper::LoadFileToString(QuirkyText, *QuirkyPath))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: missing QuirkyKeybinds.json at %s"), *QuirkyPath);
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("quirky_keybinds_missing"),
			FString::Printf(TEXT("Missing QuirkyKeybinds.json at %s"), *QuirkyPath)));
	}
	else if (!ApplyBindsJson(QuirkyText))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: failed to parse QuirkyKeybinds.json"));
		UCLErrorBoundary::ReportStatic(this, FCLError::Make(
			ECLErrorKind::NonDeterministic,
			TEXT("quirky_keybinds_parse"),
			TEXT("Failed to parse QuirkyKeybinds.json")));
		Binds = MakeDefaults();
	}
	else
	{
		Binds = MakeDefaults();
	}
	LoadFromDisk();
	if (FParse::Param(FCommandLine::Get(), TEXT("QuirkyKeybinds")))
	{
		LoadQuirkySnapshot();
	}
	EnsureComplete();
}

void UCLInputBindSubsystem::EnsureComplete()
{
	const TMap<ECLBindableAction, FCLActionBinds> Defaults = MakeDefaults();
	for (const ECLBindableAction Action : CLInput::AllActions())
	{
		if (!Binds.Contains(Action))
		{
			if (const FCLActionBinds* Def = Defaults.Find(Action))
			{
				Binds.Add(Action, *Def);
			}
			else
			{
				Binds.Add(Action, FCLActionBinds());
			}
		}
	}
}

FCLActionBinds UCLInputBindSubsystem::GetBinds(ECLBindableAction Action) const
{
	if (const FCLActionBinds* Found = Binds.Find(Action))
	{
		return *Found;
	}
	return FCLActionBinds();
}

FCLKeyChord UCLInputBindSubsystem::GetChord(ECLBindableAction Action, ECLBindColumn Column) const
{
	const FCLActionBinds Pair = GetBinds(Action);
	return ChordSlot(Pair, Column);
}

FString UCLInputBindSubsystem::GetChordDisplay(ECLBindableAction Action, ECLBindColumn Column) const
{
	return GetChord(Action, Column).ToDisplayString();
}

TArray<FCLBindUse> UCLInputBindSubsystem::FindUses(const FCLKeyChord& Chord) const
{
	TArray<FCLBindUse> Uses;
	if (!Chord.IsSet())
	{
		return Uses;
	}
	for (const TPair<ECLBindableAction, FCLActionBinds>& Pair : Binds)
	{
		if (Pair.Value.Primary.Equals(Chord))
		{
			FCLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = ECLBindColumn::Primary;
			Use.bValid = true;
			Uses.Add(Use);
		}
		if (Pair.Value.Secondary.Equals(Chord))
		{
			FCLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = ECLBindColumn::Secondary;
			Use.bValid = true;
			Uses.Add(Use);
		}
		if (Pair.Value.Gamepad.Equals(Chord))
		{
			FCLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = ECLBindColumn::Gamepad;
			Use.bValid = true;
			Uses.Add(Use);
		}
	}
	return Uses;
}

FCLBindUse UCLInputBindSubsystem::FindSameColumnUse(const FCLKeyChord& Chord, ECLBindColumn Column, ECLBindableAction ExceptAction) const
{
	FCLBindUse None;
	if (!Chord.IsSet())
	{
		return None;
	}
	for (const TPair<ECLBindableAction, FCLActionBinds>& Pair : Binds)
	{
		if (Pair.Key == ExceptAction)
		{
			continue;
		}
		const FCLKeyChord& Existing = ChordSlot(Pair.Value, Column);
		if (Existing.Equals(Chord))
		{
			FCLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = Column;
			Use.bValid = true;
			return Use;
		}
	}
	return None;
}

bool UCLInputBindSubsystem::SetBind(ECLBindableAction Action, ECLBindColumn Column, const FCLKeyChord& Chord, FCLBindUse& OutStolen)
{
	OutStolen = FCLBindUse();
	if (!Chord.IsSet() || CLInput::IsAltKey(Chord.Key) || CLInput::IsReservedMenuKey(Chord.Key))
	{
		return false;
	}

	const FCLBindUse Stolen = FindSameColumnUse(Chord, Column, Action);
	if (Stolen.bValid)
	{
		ClearBind(Stolen.Action, Stolen.Column);
		OutStolen = Stolen;
	}

	FCLActionBinds& Pair = Binds.FindOrAdd(Action);
	ChordSlot(Pair, Column) = Chord;
	SaveToDisk();
	return OutStolen.bValid;
}

void UCLInputBindSubsystem::ClearBind(ECLBindableAction Action, ECLBindColumn Column)
{
	FCLActionBinds& Pair = Binds.FindOrAdd(Action);
	ChordSlot(Pair, Column) = FCLKeyChord();
	SaveToDisk();
}

void UCLInputBindSubsystem::ResetDefaults()
{
	Binds = MakeDefaults();
	SaveToDisk();
}

FString UCLInputBindSubsystem::GetSavePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Calling"), TEXT("Input"), TEXT("Keybinds.json"));
}

FString UCLInputBindSubsystem::GetQuirkyPath()
{
	return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Input/QuirkyKeybinds.json"));
}

void UCLInputBindSubsystem::LoadQuirkySnapshot()
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *GetQuirkyPath()) || !ApplyBindsJson(Content))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: could not apply QuirkyKeybinds snapshot"));
		return;
	}
	UE_LOG(LogCalling, Display, TEXT("Calling: loaded QuirkyKeybinds snapshot"));
}

bool UCLInputBindSubsystem::ApplyBindsJson(const FString& Content)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* BindsObjPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("binds"), BindsObjPtr) || !BindsObjPtr || !BindsObjPtr->IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>& BindsObj = *BindsObjPtr;

	TMap<ECLBindableAction, FCLActionBinds> LoadedMap = MakeDefaults();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : BindsObj->Values)
	{
		ECLBindableAction Action;
		if (!CLInput::ActionFromId(Pair.Key, Action) || !Pair.Value.IsValid())
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: unknown bind action %s"), *Pair.Key);
			continue;
		}
		const TSharedPtr<FJsonObject> ActionObj = Pair.Value->AsObject();
		if (!ActionObj.IsValid())
		{
			UE_LOG(LogCalling, Warning, TEXT("Calling: bind %s is not an object"), *Pair.Key);
			continue;
		}
		FCLActionBinds Loaded = LoadedMap.FindRef(Action);
		Loaded.Primary = ChordFromJson(ActionObj->GetObjectField(TEXT("primary")));
		Loaded.Secondary = ChordFromJson(ActionObj->GetObjectField(TEXT("secondary")));
		const TSharedPtr<FJsonObject>* GamepadObj = nullptr;
		if (ActionObj->TryGetObjectField(TEXT("gamepad"), GamepadObj) && GamepadObj && GamepadObj->IsValid())
		{
			Loaded.Gamepad = ChordFromJson(*GamepadObj);
		}
		if (CLInput::IsAltKey(Loaded.Primary.Key))
		{
			Loaded.Primary = FCLKeyChord();
		}
		if (CLInput::IsAltKey(Loaded.Secondary.Key))
		{
			Loaded.Secondary = FCLKeyChord();
		}
		if (CLInput::IsAltKey(Loaded.Gamepad.Key))
		{
			Loaded.Gamepad = FCLKeyChord();
		}
		LoadedMap.Add(Action, Loaded);
	}
	Binds = MoveTemp(LoadedMap);
	return true;
}

bool UCLInputBindSubsystem::SaveToDisk() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 2);
	TSharedRef<FJsonObject> BindsObj = MakeShared<FJsonObject>();
	for (const ECLBindableAction Action : CLInput::AllActions())
	{
		const FCLActionBinds Pair = GetBinds(Action);
		TSharedRef<FJsonObject> ActionObj = MakeShared<FJsonObject>();
		ActionObj->SetObjectField(TEXT("primary"), ChordToJson(Pair.Primary));
		ActionObj->SetObjectField(TEXT("secondary"), ChordToJson(Pair.Secondary));
		ActionObj->SetObjectField(TEXT("gamepad"), ChordToJson(Pair.Gamepad));
		BindsObj->SetObjectField(CLInput::ActionId(Action), ActionObj);
	}
	Root->SetObjectField(TEXT("binds"), BindsObj);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(GetSavePath()));
	return FFileHelper::SaveStringToFile(Out, *GetSavePath());
}

void UCLInputBindSubsystem::LoadFromDisk()
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *GetSavePath()))
	{
		return;
	}
	if (!ApplyBindsJson(Content))
	{
		UE_LOG(LogCalling, Error, TEXT("Calling: failed to parse saved Keybinds.json"));
	}
}
