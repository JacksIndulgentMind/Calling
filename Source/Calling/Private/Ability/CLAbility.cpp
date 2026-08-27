#include "Ability/CLAbility.h"
#include "Dom/JsonObject.h"

void UCLAbility::Tick(float DeltaSeconds)
{
	RemainingCooldown = FMath::Max(0.f, RemainingCooldown - DeltaSeconds);
	if (ActiveSecondsRemaining > 0.f)
	{
		ActiveSecondsRemaining = FMath::Max(0.f, ActiveSecondsRemaining - DeltaSeconds);
	}
}

bool UCLAbility::CanActivate(APawn* Owner) const
{
	return Owner != nullptr && RemainingCooldown <= 0.f;
}

bool UCLAbility::Activate(APawn* Owner)
{
	if (!CanActivate(Owner))
	{
		return false;
	}
	ActiveOwner = Owner;
	ActiveSecondsRemaining = Duration;
	BeginCooldown();
	return true;
}

void UCLAbility::ApplyToMovement(APawn* Owner)
{
	(void)Owner;
}

void UCLAbility::BeginCooldown()
{
	RemainingCooldown = Cooldown;
}

void UCLAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	if (!Fields.IsValid())
	{
		return;
	}

	DisplayName = JsonString(Fields, TEXT("displayName"), DisplayName);
	RefCooldown = JsonNumber(Fields, TEXT("refCooldown"), RefCooldown);
	if (Fields->HasField(TEXT("cooldown")))
	{
		Cooldown = JsonNumber(Fields, TEXT("cooldown"), Cooldown);
	}
	else if (RefCooldown > 0.f && CooldownScale > 0.f)
	{
		Cooldown = RefCooldown * CooldownScale;
	}

	Duration = JsonNumber(Fields, TEXT("duration"), Duration);
	Damage = JsonNumber(Fields, TEXT("damage"), Damage);
	Range = JsonNumber(Fields, TEXT("range"), Range);
	Radius = JsonNumber(Fields, TEXT("radius"), Radius);
}

float UCLAbility::JsonNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Fallback)
{
	double Value = Fallback;
	return Obj.IsValid() && Obj->TryGetNumberField(Field, Value) ? static_cast<float>(Value) : Fallback;
}

bool UCLAbility::JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Fallback)
{
	bool Value = Fallback;
	return Obj.IsValid() && Obj->TryGetBoolField(Field, Value) ? Value : Fallback;
}

int32 UCLAbility::JsonInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Fallback)
{
	double Value = Fallback;
	return Obj.IsValid() && Obj->TryGetNumberField(Field, Value) ? static_cast<int32>(Value) : Fallback;
}

FString UCLAbility::JsonString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Fallback)
{
	FString Value;
	return Obj.IsValid() && Obj->TryGetStringField(Field, Value) ? Value : Fallback;
}
