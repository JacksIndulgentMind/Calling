#include "Game/CLAgentCodec.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CLAgentCodec
{
	bool JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, bool bDefault)
	{
		if (!Obj.IsValid() || !Obj->HasField(Key))
		{
			return bDefault;
		}
		const TSharedPtr<FJsonValue> Val = Obj->TryGetField(Key);
		if (!Val.IsValid())
		{
			return bDefault;
		}
		if (Val->Type == EJson::Boolean)
		{
			return Val->AsBool();
		}
		if (Val->Type == EJson::Number)
		{
			return Val->AsNumber() != 0.0;
		}
		if (Val->Type == EJson::String)
		{
			return Val->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase)
				|| Val->AsString().Equals(TEXT("1"));
		}
		return bDefault;
	}

	float JsonNum(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float Default)
	{
		return Obj.IsValid() && Obj->HasField(Key) ? static_cast<float>(Obj->GetNumberField(Key)) : Default;
	}

	FString JsonStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, const FString& Default)
	{
		FString Value;
		return Obj.IsValid() && Obj->TryGetStringField(Key, Value) ? Value : Default;
	}

	TSharedPtr<FJsonObject> JsonObj(const TSharedPtr<FJsonObject>& Root, const TCHAR* Key)
	{
		return Root.IsValid() && Root->HasField(Key) ? Root->GetObjectField(Key) : nullptr;
	}

	FString JsonToString(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	FCLLookCommand ParseLook(const TSharedPtr<FJsonObject>& LookObj)
	{
		if (!LookObj.IsValid())
		{
			return FCLLookCommand();
		}
		const bool bYawAbs = LookObj->HasField(TEXT("yawAbs"));
		const bool bPitchAbs = LookObj->HasField(TEXT("pitchAbs"));
		if (bYawAbs || bPitchAbs)
		{
			return FCLLookCommand::MakeAbsolute(
				bYawAbs, JsonNum(LookObj, TEXT("yawAbs")),
				bPitchAbs, JsonNum(LookObj, TEXT("pitchAbs")));
		}
		return FCLLookCommand::MakeDelta(JsonNum(LookObj, TEXT("yaw")), JsonNum(LookObj, TEXT("pitch")));
	}

	FCLAgentStep ParseStep(const TSharedPtr<FJsonObject>& Obj)
	{
		FCLAgentStep Step;
		if (!Obj.IsValid())
		{
			return Step;
		}
		Step.Seconds = FMath::Max(0.f, JsonNum(Obj, TEXT("seconds")));
		if (const TSharedPtr<FJsonObject> MoveObj = JsonObj(Obj, TEXT("move")))
		{
			Step.Move.X = JsonNum(MoveObj, TEXT("x"));
			Step.Move.Y = JsonNum(MoveObj, TEXT("y"));
		}
		if (const TSharedPtr<FJsonObject> LookObj = JsonObj(Obj, TEXT("look")))
		{
			Step.Look = ParseLook(LookObj);
		}
		FString TrackSeat;
		if (Obj->TryGetStringField(TEXT("lookAtSeat"), TrackSeat))
		{
			FGuid::Parse(TrackSeat, Step.TrackSeatId);
		}
		Step.bSprint = JsonBool(Obj, TEXT("sprint"));
		Step.bCrouch = JsonBool(Obj, TEXT("crouch"));
		Step.bADS = JsonBool(Obj, TEXT("ads"));
		Step.bFire = JsonBool(Obj, TEXT("fire"));
		Step.bJump = JsonBool(Obj, TEXT("jump"));
		Step.bDodge = JsonBool(Obj, TEXT("dodge"));
		Step.bDash = JsonBool(Obj, TEXT("dash"));
		Step.bReload = JsonBool(Obj, TEXT("reload"));
		Step.bSwap = JsonBool(Obj, TEXT("swap"));
		Step.bSlide = JsonBool(Obj, TEXT("slide"));
		Step.bAirDive = JsonBool(Obj, TEXT("airDive"));
		Step.bMelee = JsonBool(Obj, TEXT("melee"));
		FString Weapon;
		if (Obj->TryGetStringField(TEXT("weapon"), Weapon))
		{
			if (Weapon.Equals(TEXT("primary"), ESearchCase::IgnoreCase))
			{
				Step.bWeaponPrimary = true;
			}
			else if (Weapon.Equals(TEXT("special"), ESearchCase::IgnoreCase)
				|| Weapon.Equals(TEXT("secondary"), ESearchCase::IgnoreCase))
			{
				Step.bWeaponSpecial = true;
			}
		}
		FString Sight;
		if (Obj->TryGetStringField(TEXT("sight"), Sight))
		{
			Step.SightId = FName(*Sight);
		}
		return Step;
	}

	bool ParseSteps(const TSharedPtr<FJsonObject>& Root, TArray<FCLAgentStep>& OutSteps, bool& bRemainder)
	{
		OutSteps.Reset();
		bRemainder = false;
		if (!Root.IsValid())
		{
			return true;
		}

		FString ReplaceFrom;
		if (Root->TryGetStringField(TEXT("replaceFrom"), ReplaceFrom))
		{
			bRemainder = ReplaceFrom.Equals(TEXT("afterCurrent"), ESearchCase::IgnoreCase)
				|| ReplaceFrom.Equals(TEXT("remainder"), ESearchCase::IgnoreCase);
		}

		const TArray<TSharedPtr<FJsonValue>>* StepsJson = nullptr;
		if (!Root->TryGetArrayField(TEXT("steps"), StepsJson) || !StepsJson)
		{
			return true;
		}

		for (const TSharedPtr<FJsonValue>& Val : *StepsJson)
		{
			if (!Val.IsValid() || Val->Type != EJson::Object)
			{
				continue;
			}
			OutSteps.Add(ParseStep(Val->AsObject()));
		}
		return true;
	}

	FGuid ParseGuid(const FString& Text)
	{
		FGuid Id;
		FGuid::Parse(Text, Id);
		return Id;
	}

	FString GuidStr(const FGuid& Id)
	{
		return Id.IsValid() ? Id.ToString(EGuidFormats::DigitsWithHyphens) : FString();
	}

	FCLAgentIntent IntentFromObject(const TSharedPtr<FJsonObject>& Root, FGuid& OutTrackSeatId)
	{
		OutTrackSeatId.Invalidate();
		FCLAgentIntent Intent;
		if (!Root.IsValid())
		{
			return Intent;
		}
		if (const TSharedPtr<FJsonObject> MoveObj = JsonObj(Root, TEXT("move")))
		{
			Intent.Move.X = JsonNum(MoveObj, TEXT("x"));
			Intent.Move.Y = JsonNum(MoveObj, TEXT("y"));
		}
		FString TrackSeat;
		if (Root->TryGetStringField(TEXT("lookAtSeat"), TrackSeat))
		{
			FGuid::Parse(TrackSeat, OutTrackSeatId);
		}
		else if (const TSharedPtr<FJsonObject> LookObj = JsonObj(Root, TEXT("look")))
		{
			Intent.Look = ParseLook(LookObj).GetDelta();
		}
		Intent.bSprint = JsonBool(Root, TEXT("sprint"));
		Intent.bCrouch = JsonBool(Root, TEXT("crouch"));
		Intent.bADS = JsonBool(Root, TEXT("ads"));
		Intent.bFire = JsonBool(Root, TEXT("fire"));
		Intent.bJump = JsonBool(Root, TEXT("jump"));
		Intent.bDodge = JsonBool(Root, TEXT("dodge"));
		Intent.bDash = JsonBool(Root, TEXT("dash"));
		Intent.bReload = JsonBool(Root, TEXT("reload"));
		Intent.bSwap = JsonBool(Root, TEXT("swap"));
		Intent.bSlide = JsonBool(Root, TEXT("slide"));
		Intent.bAirDive = JsonBool(Root, TEXT("airDive"));
		Intent.bMelee = JsonBool(Root, TEXT("melee"));
		FString Weapon;
		if (Root->TryGetStringField(TEXT("weapon"), Weapon))
		{
			if (Weapon.Equals(TEXT("primary"), ESearchCase::IgnoreCase))
			{
				Intent.bWeaponPrimary = true;
			}
			else if (Weapon.Equals(TEXT("special"), ESearchCase::IgnoreCase)
				|| Weapon.Equals(TEXT("secondary"), ESearchCase::IgnoreCase))
			{
				Intent.bWeaponSpecial = true;
			}
		}
		FString Sight;
		if (Root->TryGetStringField(TEXT("sight"), Sight))
		{
			Intent.SightId = FName(*Sight);
		}
		return Intent;
	}
}
