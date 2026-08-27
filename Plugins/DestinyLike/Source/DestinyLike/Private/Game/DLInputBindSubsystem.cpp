#include "Game/DLInputBindSubsystem.h"
#include "Core/DLLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"

namespace
{
	TSharedRef<FJsonObject> ChordToJson(const FDLKeyChord& Chord)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		if (Chord.IsSet())
		{
			Obj->SetStringField(TEXT("key"), Chord.Key.ToString());
			Obj->SetBoolField(TEXT("alt"), Chord.bAlt);
		}
		return Obj;
	}

	FDLKeyChord ChordFromJson(const TSharedPtr<FJsonObject>& Obj)
	{
		FDLKeyChord Chord;
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
		if (DLInput::IsAltKey(Chord.Key))
		{
			return FDLKeyChord();
		}
		return Chord;
	}

	FDLKeyChord MakeChord(const FKey& Key, bool bAlt = false)
	{
		FDLKeyChord Chord;
		Chord.Key = Key;
		Chord.bAlt = bAlt;
		return Chord;
	}

	FDLKeyChord& ChordSlot(FDLActionBinds& Pair, EDLBindColumn Column)
	{
		switch (Column)
		{
		case EDLBindColumn::Primary: return Pair.Primary;
		case EDLBindColumn::Secondary: return Pair.Secondary;
		default: return Pair.Gamepad;
		}
	}

	const FDLKeyChord& ChordSlot(const FDLActionBinds& Pair, EDLBindColumn Column)
	{
		switch (Column)
		{
		case EDLBindColumn::Primary: return Pair.Primary;
		case EDLBindColumn::Secondary: return Pair.Secondary;
		default: return Pair.Gamepad;
		}
	}
}

TMap<EDLBindableAction, FDLActionBinds> UDLInputBindSubsystem::MakeDefaults()
{
	TMap<EDLBindableAction, FDLActionBinds> Out;
	auto Set = [&Out](EDLBindableAction Action, const FKey& Primary, const FKey& Secondary = EKeys::Invalid, const FKey& Gamepad = EKeys::Invalid)
	{
		FDLActionBinds Binds;
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

	Set(EDLBindableAction::Fire, EKeys::LeftMouseButton, EKeys::Invalid, EKeys::Gamepad_RightTrigger);
	Set(EDLBindableAction::ADS, EKeys::RightMouseButton, EKeys::Invalid, EKeys::Gamepad_LeftTrigger);
	Set(EDLBindableAction::Reload, EKeys::R, EKeys::Invalid, EKeys::Gamepad_FaceButton_Left);
	Set(EDLBindableAction::Swap, EKeys::Q, EKeys::Invalid, EKeys::Gamepad_FaceButton_Top);
	Set(EDLBindableAction::WeaponPrimary, EKeys::One);
	Set(EDLBindableAction::WeaponSpecial, EKeys::Two);
	Set(EDLBindableAction::Jump, EKeys::SpaceBar, EKeys::Invalid, EKeys::Gamepad_FaceButton_Bottom);
	Set(EDLBindableAction::Sprint, EKeys::LeftShift, EKeys::Invalid, EKeys::Gamepad_LeftThumbstick);
	Set(EDLBindableAction::Crouch, EKeys::C, EKeys::Invalid, EKeys::Gamepad_FaceButton_Right);
	Set(EDLBindableAction::Slide, EKeys::LeftControl);
	Set(EDLBindableAction::AirDive, EKeys::Z, EKeys::Invalid, EKeys::Gamepad_DPad_Down);
	Set(EDLBindableAction::Dodge, EKeys::X);
	Set(EDLBindableAction::Grenade, EKeys::B, EKeys::Invalid, EKeys::Gamepad_LeftShoulder);
	Set(EDLBindableAction::Melee, EKeys::F, EKeys::Invalid, EKeys::Gamepad_RightShoulder);
	Set(EDLBindableAction::Dash, EKeys::V, EKeys::Invalid, EKeys::Gamepad_DPad_Up);
	Set(EDLBindableAction::Shield, EKeys::G, EKeys::Invalid, EKeys::Gamepad_DPad_Left);
	Set(EDLBindableAction::Evasion, EKeys::E, EKeys::Invalid, EKeys::Gamepad_DPad_Right);
	Set(EDLBindableAction::Super, EKeys::T);
	return Out;
}

void UDLInputBindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Binds = MakeDefaults();
	FString QuirkyText;
	const FString QuirkyPath = GetQuirkyPath();
	if (!FFileHelper::LoadFileToString(QuirkyText, *QuirkyPath))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: missing QuirkyKeybinds.json at %s"), *QuirkyPath);
	}
	else if (!ApplyBindsJson(QuirkyText))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: failed to parse QuirkyKeybinds.json"));
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

void UDLInputBindSubsystem::EnsureComplete()
{
	const TMap<EDLBindableAction, FDLActionBinds> Defaults = MakeDefaults();
	for (const EDLBindableAction Action : DLInput::AllActions())
	{
		if (!Binds.Contains(Action))
		{
			if (const FDLActionBinds* Def = Defaults.Find(Action))
			{
				Binds.Add(Action, *Def);
			}
			else
			{
				Binds.Add(Action, FDLActionBinds());
			}
		}
	}
}

FDLActionBinds UDLInputBindSubsystem::GetBinds(EDLBindableAction Action) const
{
	if (const FDLActionBinds* Found = Binds.Find(Action))
	{
		return *Found;
	}
	return FDLActionBinds();
}

FDLKeyChord UDLInputBindSubsystem::GetChord(EDLBindableAction Action, EDLBindColumn Column) const
{
	const FDLActionBinds Pair = GetBinds(Action);
	return ChordSlot(Pair, Column);
}

FString UDLInputBindSubsystem::GetChordDisplay(EDLBindableAction Action, EDLBindColumn Column) const
{
	return GetChord(Action, Column).ToDisplayString();
}

TArray<FDLBindUse> UDLInputBindSubsystem::FindUses(const FDLKeyChord& Chord) const
{
	TArray<FDLBindUse> Uses;
	if (!Chord.IsSet())
	{
		return Uses;
	}
	for (const TPair<EDLBindableAction, FDLActionBinds>& Pair : Binds)
	{
		if (Pair.Value.Primary.Equals(Chord))
		{
			FDLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = EDLBindColumn::Primary;
			Use.bValid = true;
			Uses.Add(Use);
		}
		if (Pair.Value.Secondary.Equals(Chord))
		{
			FDLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = EDLBindColumn::Secondary;
			Use.bValid = true;
			Uses.Add(Use);
		}
		if (Pair.Value.Gamepad.Equals(Chord))
		{
			FDLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = EDLBindColumn::Gamepad;
			Use.bValid = true;
			Uses.Add(Use);
		}
	}
	return Uses;
}

FDLBindUse UDLInputBindSubsystem::FindSameColumnUse(const FDLKeyChord& Chord, EDLBindColumn Column, EDLBindableAction ExceptAction) const
{
	FDLBindUse None;
	if (!Chord.IsSet())
	{
		return None;
	}
	for (const TPair<EDLBindableAction, FDLActionBinds>& Pair : Binds)
	{
		if (Pair.Key == ExceptAction)
		{
			continue;
		}
		const FDLKeyChord& Existing = ChordSlot(Pair.Value, Column);
		if (Existing.Equals(Chord))
		{
			FDLBindUse Use;
			Use.Action = Pair.Key;
			Use.Column = Column;
			Use.bValid = true;
			return Use;
		}
	}
	return None;
}

bool UDLInputBindSubsystem::SetBind(EDLBindableAction Action, EDLBindColumn Column, const FDLKeyChord& Chord, FDLBindUse& OutStolen)
{
	OutStolen = FDLBindUse();
	if (!Chord.IsSet() || DLInput::IsAltKey(Chord.Key) || DLInput::IsReservedMenuKey(Chord.Key))
	{
		return false;
	}

	const FDLBindUse Stolen = FindSameColumnUse(Chord, Column, Action);
	if (Stolen.bValid)
	{
		ClearBind(Stolen.Action, Stolen.Column);
		OutStolen = Stolen;
	}

	FDLActionBinds& Pair = Binds.FindOrAdd(Action);
	ChordSlot(Pair, Column) = Chord;
	SaveToDisk();
	return OutStolen.bValid;
}

void UDLInputBindSubsystem::ClearBind(EDLBindableAction Action, EDLBindColumn Column)
{
	FDLActionBinds& Pair = Binds.FindOrAdd(Action);
	ChordSlot(Pair, Column) = FDLKeyChord();
	SaveToDisk();
}

void UDLInputBindSubsystem::ResetDefaults()
{
	Binds = MakeDefaults();
	SaveToDisk();
}

FString UDLInputBindSubsystem::GetSavePath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DestinyLike"), TEXT("Input"), TEXT("Keybinds.json"));
}

FString UDLInputBindSubsystem::GetQuirkyPath()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DestinyLike")))
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/Input/QuirkyKeybinds.json"));
	}
	return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("DestinyLike/Config/Input/QuirkyKeybinds.json"));
}

void UDLInputBindSubsystem::LoadQuirkySnapshot()
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *GetQuirkyPath()) || !ApplyBindsJson(Content))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: could not apply QuirkyKeybinds snapshot"));
		return;
	}
	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike: loaded QuirkyKeybinds snapshot"));
}

bool UDLInputBindSubsystem::ApplyBindsJson(const FString& Content)
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

	TMap<EDLBindableAction, FDLActionBinds> LoadedMap = MakeDefaults();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : BindsObj->Values)
	{
		EDLBindableAction Action;
		if (!DLInput::ActionFromId(Pair.Key, Action) || !Pair.Value.IsValid())
		{
			UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: unknown bind action %s"), *Pair.Key);
			continue;
		}
		const TSharedPtr<FJsonObject> ActionObj = Pair.Value->AsObject();
		if (!ActionObj.IsValid())
		{
			UE_LOG(LogDestinyLike, Warning, TEXT("DestinyLike: bind %s is not an object"), *Pair.Key);
			continue;
		}
		FDLActionBinds Loaded = LoadedMap.FindRef(Action);
		Loaded.Primary = ChordFromJson(ActionObj->GetObjectField(TEXT("primary")));
		Loaded.Secondary = ChordFromJson(ActionObj->GetObjectField(TEXT("secondary")));
		const TSharedPtr<FJsonObject>* GamepadObj = nullptr;
		if (ActionObj->TryGetObjectField(TEXT("gamepad"), GamepadObj) && GamepadObj && GamepadObj->IsValid())
		{
			Loaded.Gamepad = ChordFromJson(*GamepadObj);
		}
		if (DLInput::IsAltKey(Loaded.Primary.Key))
		{
			Loaded.Primary = FDLKeyChord();
		}
		if (DLInput::IsAltKey(Loaded.Secondary.Key))
		{
			Loaded.Secondary = FDLKeyChord();
		}
		if (DLInput::IsAltKey(Loaded.Gamepad.Key))
		{
			Loaded.Gamepad = FDLKeyChord();
		}
		LoadedMap.Add(Action, Loaded);
	}
	Binds = MoveTemp(LoadedMap);
	return true;
}

bool UDLInputBindSubsystem::SaveToDisk() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 2);
	TSharedRef<FJsonObject> BindsObj = MakeShared<FJsonObject>();
	for (const EDLBindableAction Action : DLInput::AllActions())
	{
		const FDLActionBinds Pair = GetBinds(Action);
		TSharedRef<FJsonObject> ActionObj = MakeShared<FJsonObject>();
		ActionObj->SetObjectField(TEXT("primary"), ChordToJson(Pair.Primary));
		ActionObj->SetObjectField(TEXT("secondary"), ChordToJson(Pair.Secondary));
		ActionObj->SetObjectField(TEXT("gamepad"), ChordToJson(Pair.Gamepad));
		BindsObj->SetObjectField(DLInput::ActionId(Action), ActionObj);
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

void UDLInputBindSubsystem::LoadFromDisk()
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *GetSavePath()))
	{
		return;
	}
	if (!ApplyBindsJson(Content))
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike: failed to parse saved Keybinds.json"));
	}
}
