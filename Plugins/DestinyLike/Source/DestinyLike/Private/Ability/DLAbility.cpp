#include "Ability/DLAbility.h"
#include "Dom/JsonObject.h"

void UDLAbility::Tick(float DeltaSeconds)
{
	RemainingCooldown = FMath::Max(0.f, RemainingCooldown - DeltaSeconds);
	if (ActiveSecondsRemaining > 0.f)
	{
		ActiveSecondsRemaining = FMath::Max(0.f, ActiveSecondsRemaining - DeltaSeconds);
	}
}

bool UDLAbility::CanActivate(APawn* Owner) const
{
	return Owner != nullptr && RemainingCooldown <= 0.f;
}

bool UDLAbility::Activate(APawn* Owner)
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

void UDLAbility::ApplyToMovement(APawn* Owner)
{
	(void)Owner;
}

void UDLAbility::BeginCooldown()
{
	RemainingCooldown = Cooldown;
}

void UDLAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
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

float UDLAbility::JsonNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float Fallback)
{
	double Value = Fallback;
	return Obj.IsValid() && Obj->TryGetNumberField(Field, Value) ? static_cast<float>(Value) : Fallback;
}

bool UDLAbility::JsonBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Fallback)
{
	bool Value = Fallback;
	return Obj.IsValid() && Obj->TryGetBoolField(Field, Value) ? Value : Fallback;
}

int32 UDLAbility::JsonInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Fallback)
{
	double Value = Fallback;
	return Obj.IsValid() && Obj->TryGetNumberField(Field, Value) ? static_cast<int32>(Value) : Fallback;
}

FString UDLAbility::JsonString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Fallback)
{
	FString Value;
	return Obj.IsValid() && Obj->TryGetStringField(Field, Value) ? Value : Fallback;
}
