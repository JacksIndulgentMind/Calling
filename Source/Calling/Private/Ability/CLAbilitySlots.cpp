#include "Ability/CLAbilitySlots.h"
#include "Dom/JsonObject.h"

void UCLGrenadeAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	LingerSeconds = JsonNumber(Fields, TEXT("lingerSeconds"), LingerSeconds);
	BurnDamagePerSecond = JsonNumber(Fields, TEXT("burnDamagePerSecond"), BurnDamagePerSecond);
	bBurn = JsonBool(Fields, TEXT("burn"), bBurn);
	bLinger = JsonBool(Fields, TEXT("linger"), bLinger);
	bSpawnSeekers = JsonBool(Fields, TEXT("spawnSeekers"), bSpawnSeekers);
	SeekerCount = JsonInt(Fields, TEXT("seekerCount"), SeekerCount);
}

void UCLShieldAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	HoverSeconds = JsonNumber(Fields, TEXT("hoverSeconds"), HoverSeconds);
	InterceptChance = JsonNumber(Fields, TEXT("interceptChance"), InterceptChance);
}

void UCLEvasionAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	DamageTakenMultiplier = JsonNumber(Fields, TEXT("damageTakenMultiplier"), DamageTakenMultiplier);
}

void UCLDashAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	AirControl = JsonNumber(Fields, TEXT("airControl"), AirControl);
	HoverSeconds = JsonNumber(Fields, TEXT("hoverSeconds"), HoverSeconds);
}

void UCLMeleeAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	HealSelf = JsonNumber(Fields, TEXT("healSelf"), HealSelf);
}

bool UCLJumpAbility::CanActivate(APawn* Owner) const
{
	(void)Owner;
	return false;
}

bool UCLJumpAbility::Activate(APawn* Owner)
{
	(void)Owner;
	return false;
}

void UCLJumpAbility::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	AirControl = JsonNumber(Fields, TEXT("airControl"), AirControl);
	SecondJumpZ = JsonNumber(Fields, TEXT("secondJumpZ"), SecondJumpZ);
	HoverSeconds = JsonNumber(Fields, TEXT("hoverSeconds"), HoverSeconds);
	bAllowSecondJumpFromGround = JsonBool(Fields, TEXT("allowSecondJumpFromGround"), bAllowSecondJumpFromGround);
}
