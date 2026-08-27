#include "Ability/DLAbilityConcrete.h"
#include "Ability/DLAbilityTypeRegistry.h"
#include "Ability/DLAbilityCombat.h"
#include "Ability/DLAbilityWorld.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLHealthShieldComponent.h"
#include "Ability/DLAbilityTypes.h"
#include "Game/DLLobbySubsystem.h"
#include "Game/DLParticipantSeat.h"
#include "Game/DLControllerPlaybook.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"

static ADLPlayerCharacter* DLChar(APawn* Owner)
{
	return Cast<ADLPlayerCharacter>(Owner);
}

bool UDLGrenade_ThrownAoE::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner || !Owner->GetWorld())
	{
		return false;
	}

	const FVector Start = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 80.f + FVector(0.f, 0.f, 40.f);
	FHitResult Hit;
	const FVector End = Start + Owner->GetActorForwardVector() * 1800.f - FVector(0.f, 0.f, 400.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DLGrenadeArc), false, Owner);
	const FVector Impact = Owner->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params)
		? Hit.ImpactPoint
		: End;

	if (bSpawnSeekers && SeekerCount > 0)
	{
		for (int32 i = 0; i < SeekerCount; ++i)
		{
			const FVector Offset = Owner->GetActorRightVector() * (i - 1) * 40.f;
			if (ADLAbilitySeeker* Seeker = Owner->GetWorld()->SpawnActor<ADLAbilitySeeker>(Impact + Offset + FVector(0, 0, 40.f), FRotator::ZeroRotator))
			{
				Seeker->InitSeeker(Owner, Damage, Radius > 0.f ? Radius : 180.f, 4.f);
			}
		}
		return true;
	}

	const float Life = bLinger ? FMath::Max(LingerSeconds, Duration) : 0.35f;
	if (ADLAbilityAoE* AoE = Owner->GetWorld()->SpawnActor<ADLAbilityAoE>(Impact + FVector(0, 0, 20.f), FRotator::ZeroRotator))
	{
		AoE->InitAoE(Owner, Radius > 0.f ? Radius : 400.f, Life, Damage, bBurn ? BurnDamagePerSecond : (bLinger ? Damage * 0.25f : 0.f));
	}
	return true;
}

bool UDLMelee_Lunge::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		Char->PlayKnifeSlash();
	}
	FHitResult Hit;
	if (DLAbilityCombat::TraceForward(Owner, Range > 0.f ? Range : 220.f, Hit))
	{
		DLAbilityCombat::ApplyDamageToActor(Hit.GetActor(), Owner, Damage);
	}
	if (HealSelf > 0.f)
	{
		if (ADLPlayerCharacter* Char = DLChar(Owner))
		{
			if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->Heal(HealSelf);
			}
		}
	}
	return true;
}

bool UDLDash_Lunge::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDash(Char->GetActorForwardVector());
		}
	}
	FHitResult Hit;
	const float LungeHit = Range > 0.f ? Range : 250.f;
	if (Damage > 0.f && DLAbilityCombat::TraceForward(Owner, LungeHit, Hit))
	{
		DLAbilityCombat::ApplyDamageToActor(Hit.GetActor(), Owner, Damage);
	}
	return true;
}

bool UDLDash_BlinkStep::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner)
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDash(Owner->GetActorForwardVector());
		}
	}
	return true;
}

bool UDLDash_AirThrust::CanActivate(APawn* Owner) const
{
	if (!Super::CanActivate(Owner))
	{
		return false;
	}
	if (const ADLPlayerCharacter* Char = Cast<ADLPlayerCharacter>(Owner))
	{
		if (const UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			return !Move->IsMovingOnGround();
		}
	}
	return false;
}

bool UDLDash_AirThrust::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDash(Char->GetActorForwardVector());
		}
	}
	return true;
}

void UDLJump_RocketPulse::ApplyToMovement(APawn* Owner)
{
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->SetJumpStyle(EDLJumpStyle::RocketPulse);
			Move->SetAllowSecondJumpFromGround(false);
			Move->SetHoverSeconds(0.f);
			if (AirControl >= 0.f)
			{
				Move->AirControl = AirControl;
			}
			if (SecondJumpZ > 0.f)
			{
				Move->SetSecondJumpZ(SecondJumpZ);
			}
		}
	}
}

void UDLJump_InertiaDampers::ApplyToMovement(APawn* Owner)
{
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->SetJumpStyle(EDLJumpStyle::InertiaDamp);
			Move->SetAllowSecondJumpFromGround(bAllowSecondJumpFromGround);
			Move->SetHoverSeconds(HoverSeconds > 0.f ? HoverSeconds : 1.4f);
			if (AirControl >= 0.f)
			{
				Move->AirControl = AirControl;
			}
			if (SecondJumpZ > 0.f)
			{
				Move->SetSecondJumpZ(SecondJumpZ);
			}
		}
	}
}

bool UDLShield_Deployable::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner || !Owner->GetWorld())
	{
		return false;
	}
	const FVector Loc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 90.f;
	if (ADLAbilityBarricade* Wall = Owner->GetWorld()->SpawnActor<ADLAbilityBarricade>(Loc, Owner->GetActorRotation()))
	{
		Wall->InitBarricade(Duration > 0.f ? Duration : 6.f);
	}
	return true;
}

bool UDLShield_LightADS::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			HS->SetInterceptChance(InterceptChance > 0.f ? InterceptChance : 0.35f);
			HS->RestoreShield(25.f);
		}
	}
	return true;
}

void UDLShield_LightADS::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ADLPlayerCharacter* Char = DLChar(ActiveOwner.Get()))
		{
			if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->SetInterceptChance(0.f);
			}
		}
	}
}

bool UDLShield_InterceptorDrones::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			HS->SetInterceptChance(InterceptChance > 0.f ? InterceptChance : 0.75f);
		}
	}
	return true;
}

void UDLShield_InterceptorDrones::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (APawn* Owner = ActiveOwner.Get())
	{
		if (ActiveSecondsRemaining > 0.f)
		{
			DrawDebugSphere(Owner->GetWorld(), Owner->GetActorLocation() + Owner->GetActorForwardVector() * 60.f, 18.f, 8, FColor::Cyan, false, 0.f);
		}
	}
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ADLPlayerCharacter* Char = DLChar(ActiveOwner.Get()))
		{
			if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->SetInterceptChance(0.f);
			}
		}
	}
}

bool UDLEvasion_Fortify::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			HS->SetDamageTakenMultiplier(DamageTakenMultiplier > 0.f ? DamageTakenMultiplier : 0.35f);
			HS->RestoreShield(40.f);
		}
	}
	return true;
}

void UDLEvasion_Fortify::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ADLPlayerCharacter* Char = DLChar(ActiveOwner.Get()))
		{
			if (UDLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->SetDamageTakenMultiplier(1.f);
			}
		}
	}
}

bool UDLEvasion_RippleCamo::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ADLPlayerCharacter* Char = DLChar(Owner))
	{
		Char->SetRippleCamo(true);
	}
	return true;
}

void UDLEvasion_RippleCamo::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ADLPlayerCharacter* Char = DLChar(ActiveOwner.Get()))
		{
			Char->SetRippleCamo(false);
		}
	}
}

bool UDLEvasion_Superposition::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner || !Owner->GetWorld())
	{
		return false;
	}
	bEndedSmear = false;
	ForesightLeft = 0.f;
	const FVector Base = Owner->GetActorLocation();
	const FVector Right = Owner->GetActorRightVector();
	const FVector Fwd = Owner->GetActorForwardVector();
	const FVector Offsets[] = { Right * 80.f, Right * -80.f, Fwd * 70.f, Fwd * -50.f + Right * 40.f };
	const float Life = Duration > 0.f ? Duration : 3.f;
	for (const FVector& Off : Offsets)
	{
		if (ADLAbilityDecoy* Ghost = Owner->GetWorld()->SpawnActor<ADLAbilityDecoy>(Base + Off, Owner->GetActorRotation()))
		{
			Ghost->InitDecoy(Life);
		}
	}
	return true;
}

void UDLEvasion_Superposition::Tick(float DeltaSeconds)
{
	const bool bWasSmear = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasSmear && ActiveSecondsRemaining <= 0.f && !bEndedSmear)
	{
		bEndedSmear = true;
		ForesightLeft = ForesightSeconds > 0.f ? ForesightSeconds : 3.f;
	}
	if (ForesightLeft > 0.f)
	{
		ForesightLeft = FMath::Max(0.f, ForesightLeft - DeltaSeconds);
		if (APawn* Owner = ActiveOwner.Get())
		{
			for (TActorIterator<APawn> It(Owner->GetWorld()); It; ++It)
			{
				APawn* Other = *It;
				if (!Other || Other == Owner)
				{
					continue;
				}
				DrawDebugSphere(Owner->GetWorld(), Other->GetActorLocation(), 40.f, 8, FColor::Orange, false, 0.f);
			}
		}
	}
}

void UDLEvasion_Superposition::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	ForesightSeconds = JsonNumber(Fields, TEXT("foresightSeconds"), ForesightSeconds);
}

bool UDLSuper_MindControl::CanActivate(APawn* Owner) const
{
	return Super::CanActivate(Owner);
}

bool UDLSuper_MindControl::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner)
	{
		return false;
	}
	UWorld* World = Owner->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UDLLobbySubsystem>() : nullptr;
	if (!Lobby)
	{
		return false;
	}

	FGuid AgentSeat;
	FGuid TargetSeat;
	for (UDLParticipantSeat* Seat : Lobby->GetSeats())
	{
		if (!Seat)
		{
			continue;
		}
		if (Seat->GetDrivenPawn() == Owner && Seat->GetPlaybook() && Seat->GetPlaybook()->IsA<UDLRemoteAgentPlaybook>())
		{
			AgentSeat = Seat->GetSeatId();
		}
		else if (!TargetSeat.IsValid() && Seat->GetDrivenPawn() && Seat->GetDrivenPawn() != Owner)
		{
			TargetSeat = Seat->GetSeatId();
		}
	}
	FString Error;
	return Lobby->MindControl(AgentSeat, TargetSeat, Error);
}

namespace
{
	struct FRegisterAbilityTypes
	{
		FRegisterAbilityTypes()
		{
			FDLAbilityTypeRegistry::Register(TEXT("DLGrenade_ThrownAoE"), UDLGrenade_ThrownAoE::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLMelee_Lunge"), UDLMelee_Lunge::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLDash_Lunge"), UDLDash_Lunge::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLDash_BlinkStep"), UDLDash_BlinkStep::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLDash_AirThrust"), UDLDash_AirThrust::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLJump_RocketPulse"), UDLJump_RocketPulse::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLJump_InertiaDampers"), UDLJump_InertiaDampers::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLShield_Deployable"), UDLShield_Deployable::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLShield_LightADS"), UDLShield_LightADS::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLShield_InterceptorDrones"), UDLShield_InterceptorDrones::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLEvasion_Fortify"), UDLEvasion_Fortify::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLEvasion_RippleCamo"), UDLEvasion_RippleCamo::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLEvasion_Superposition"), UDLEvasion_Superposition::StaticClass());
			FDLAbilityTypeRegistry::Register(TEXT("DLSuper_MindControl"), UDLSuper_MindControl::StaticClass());
		}
	};
	static FRegisterAbilityTypes GRegisterAbilityTypes;
}
