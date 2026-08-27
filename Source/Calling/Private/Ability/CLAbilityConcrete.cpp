#include "Ability/CLAbilityConcrete.h"
#include "Ability/CLAbilityTypeRegistry.h"
#include "Ability/CLAbilityCombat.h"
#include "Ability/CLAbilityWorld.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLHealthShieldComponent.h"
#include "Ability/CLAbilityTypes.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLControllerPlaybook.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"

static ACLPlayerCharacter* CLChar(APawn* Owner)
{
	return Cast<ACLPlayerCharacter>(Owner);
}

bool UCLGrenade_ThrownAoE::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner || !Owner->GetWorld())
	{
		return false;
	}

	const FVector Start = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 80.f + FVector(0.f, 0.f, 40.f);
	FHitResult Hit;
	const FVector End = Start + Owner->GetActorForwardVector() * 1800.f - FVector(0.f, 0.f, 400.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CLGrenadeArc), false, Owner);
	const FVector Impact = Owner->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params)
		? Hit.ImpactPoint
		: End;

	if (bSpawnSeekers && SeekerCount > 0)
	{
		for (int32 i = 0; i < SeekerCount; ++i)
		{
			const FVector Offset = Owner->GetActorRightVector() * (i - 1) * 40.f;
			if (ACLAbilitySeeker* Seeker = Owner->GetWorld()->SpawnActor<ACLAbilitySeeker>(Impact + Offset + FVector(0, 0, 40.f), FRotator::ZeroRotator))
			{
				Seeker->InitSeeker(Owner, Damage, Radius > 0.f ? Radius : 180.f, 4.f);
			}
		}
		return true;
	}

	const float Life = bLinger ? FMath::Max(LingerSeconds, Duration) : 0.35f;
	if (ACLAbilityAoE* AoE = Owner->GetWorld()->SpawnActor<ACLAbilityAoE>(Impact + FVector(0, 0, 20.f), FRotator::ZeroRotator))
	{
		AoE->InitAoE(Owner, Radius > 0.f ? Radius : 400.f, Life, Damage, bBurn ? BurnDamagePerSecond : (bLinger ? Damage * 0.25f : 0.f));
	}
	return true;
}

bool UCLMelee_Lunge::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		Char->PlayKnifeSlash();
	}
	FHitResult Hit;
	if (CLAbilityCombat::TraceForward(Owner, Range > 0.f ? Range : 220.f, Hit))
	{
		CLAbilityCombat::ApplyDamageToActor(Hit.GetActor(), Owner, Damage);
	}
	if (HealSelf > 0.f)
	{
		if (ACLPlayerCharacter* Char = CLChar(Owner))
		{
			if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->Heal(HealSelf);
			}
		}
	}
	return true;
}

bool UCLDash_Lunge::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDash(Char->GetActorForwardVector());
		}
	}
	FHitResult Hit;
	const float LungeHit = Range > 0.f ? Range : 250.f;
	if (Damage > 0.f && CLAbilityCombat::TraceForward(Owner, LungeHit, Hit))
	{
		CLAbilityCombat::ApplyDamageToActor(Hit.GetActor(), Owner, Damage);
	}
	return true;
}

bool UCLDash_BlinkStep::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner)
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDash(Owner->GetActorForwardVector());
		}
	}
	return true;
}

bool UCLDash_AirThrust::CanActivate(APawn* Owner) const
{
	if (!Super::CanActivate(Owner))
	{
		return false;
	}
	if (const ACLPlayerCharacter* Char = Cast<ACLPlayerCharacter>(Owner))
	{
		if (const UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			return !Move->IsMovingOnGround();
		}
	}
	return false;
}

bool UCLDash_AirThrust::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->TryDash(Char->GetActorForwardVector());
		}
	}
	return true;
}

void UCLJump_RocketPulse::ApplyToMovement(APawn* Owner)
{
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->SetJumpStyle(ECLJumpStyle::RocketPulse);
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

void UCLJump_InertiaDampers::ApplyToMovement(APawn* Owner)
{
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->SetJumpStyle(ECLJumpStyle::InertiaDamp);
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

bool UCLShield_Deployable::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner || !Owner->GetWorld())
	{
		return false;
	}
	const FVector Loc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 90.f;
	if (ACLAbilityBarricade* Wall = Owner->GetWorld()->SpawnActor<ACLAbilityBarricade>(Loc, Owner->GetActorRotation()))
	{
		Wall->InitBarricade(Duration > 0.f ? Duration : 6.f);
	}
	return true;
}

bool UCLShield_LightADS::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			HS->SetInterceptChance(InterceptChance > 0.f ? InterceptChance : 0.35f);
			HS->RestoreShield(25.f);
		}
	}
	return true;
}

void UCLShield_LightADS::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ACLPlayerCharacter* Char = CLChar(ActiveOwner.Get()))
		{
			if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->SetInterceptChance(0.f);
			}
		}
	}
}

bool UCLShield_InterceptorDrones::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			HS->SetInterceptChance(InterceptChance > 0.f ? InterceptChance : 0.75f);
		}
	}
	return true;
}

void UCLShield_InterceptorDrones::Tick(float DeltaSeconds)
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
		if (ACLPlayerCharacter* Char = CLChar(ActiveOwner.Get()))
		{
			if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->SetInterceptChance(0.f);
			}
		}
	}
}

bool UCLEvasion_Fortify::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
		{
			HS->SetDamageTakenMultiplier(DamageTakenMultiplier > 0.f ? DamageTakenMultiplier : 0.35f);
			HS->RestoreShield(40.f);
		}
	}
	return true;
}

void UCLEvasion_Fortify::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ACLPlayerCharacter* Char = CLChar(ActiveOwner.Get()))
		{
			if (UCLHealthShieldComponent* HS = Char->GetHealthShield())
			{
				HS->SetDamageTakenMultiplier(1.f);
			}
		}
	}
}

bool UCLEvasion_RippleCamo::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner))
	{
		return false;
	}
	if (ACLPlayerCharacter* Char = CLChar(Owner))
	{
		Char->SetRippleCamo(true);
	}
	return true;
}

void UCLEvasion_RippleCamo::Tick(float DeltaSeconds)
{
	const bool bWasActive = ActiveSecondsRemaining > 0.f;
	Super::Tick(DeltaSeconds);
	if (bWasActive && ActiveSecondsRemaining <= 0.f)
	{
		if (ACLPlayerCharacter* Char = CLChar(ActiveOwner.Get()))
		{
			Char->SetRippleCamo(false);
		}
	}
}

bool UCLEvasion_Superposition::Activate(APawn* Owner)
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
		if (ACLAbilityDecoy* Ghost = Owner->GetWorld()->SpawnActor<ACLAbilityDecoy>(Base + Off, Owner->GetActorRotation()))
		{
			Ghost->InitDecoy(Life);
		}
	}
	return true;
}

void UCLEvasion_Superposition::Tick(float DeltaSeconds)
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

void UCLEvasion_Superposition::ApplyTuning(const TSharedPtr<FJsonObject>& Fields, float CooldownScale)
{
	Super::ApplyTuning(Fields, CooldownScale);
	ForesightSeconds = JsonNumber(Fields, TEXT("foresightSeconds"), ForesightSeconds);
}

bool UCLSuper_MindControl::CanActivate(APawn* Owner) const
{
	return Super::CanActivate(Owner);
}

bool UCLSuper_MindControl::Activate(APawn* Owner)
{
	if (!Super::Activate(Owner) || !Owner)
	{
		return false;
	}
	UWorld* World = Owner->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	if (!Lobby)
	{
		return false;
	}

	FGuid AgentSeat;
	FGuid TargetSeat;
	for (UCLParticipantSeat* Seat : Lobby->GetSeats())
	{
		if (!Seat)
		{
			continue;
		}
		if (Seat->GetDrivenPawn() == Owner && Seat->GetPlaybook() && Seat->GetPlaybook()->IsA<UCLRemoteAgentPlaybook>())
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
			FCLAbilityTypeRegistry::Register(TEXT("CLGrenade_ThrownAoE"), UCLGrenade_ThrownAoE::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLMelee_Lunge"), UCLMelee_Lunge::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLDash_Lunge"), UCLDash_Lunge::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLDash_BlinkStep"), UCLDash_BlinkStep::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLDash_AirThrust"), UCLDash_AirThrust::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLJump_RocketPulse"), UCLJump_RocketPulse::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLJump_InertiaDampers"), UCLJump_InertiaDampers::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLShield_Deployable"), UCLShield_Deployable::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLShield_LightADS"), UCLShield_LightADS::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLShield_InterceptorDrones"), UCLShield_InterceptorDrones::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLEvasion_Fortify"), UCLEvasion_Fortify::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLEvasion_RippleCamo"), UCLEvasion_RippleCamo::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLEvasion_Superposition"), UCLEvasion_Superposition::StaticClass());
			FCLAbilityTypeRegistry::Register(TEXT("CLSuper_MindControl"), UCLSuper_MindControl::StaticClass());
		}
	};
	static FRegisterAbilityTypes GRegisterAbilityTypes;
}
