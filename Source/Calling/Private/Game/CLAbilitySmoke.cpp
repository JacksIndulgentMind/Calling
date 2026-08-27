#include "Game/CLAbilitySmoke.h"
#include "Game/CLGameModeBase.h"
#include "Core/CLLog.h"
#include "Game/CLGameStateBase.h"
#include "Game/CLActivityStateComponent.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLVaultSubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLGameInstance.h"
#include "Game/CLSessionSubsystem.h"
#include "Loot/CLLootRulesService.h"
#include "AI/CLEncounterDirector.h"
#include "AI/CLPracticeDummy.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLHealthShieldComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Player/CLPlayerController.h"
#include "Player/CLVanguardCharacter.h"
#include "Player/CLPathfinderCharacter.h"
#include "Player/CLWardenCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Ability/CLAbilityCatalog.h"
#include "Ability/CLAbility.h"
#include "HAL/IConsoleManager.h"
#include "UI/CLBootProfileWidget.h"
#include "UI/CLSocialMarkerWidget.h"
#include "Game/CLGreyboxFloors.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/PlayerStartPIE.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static TAutoConsoleVariable<int32> CVarDLAbilitySmoke(
	TEXT("dl.AbilitySmoke"),
	0,
	TEXT("If 1, spawn each class pawn and exercise sprint/slide/dodge/jump plus fireable slots."));

bool CLShouldRunAbilitySmoke()
{
	return CVarDLAbilitySmoke.GetValueOnGameThread() > 0 || FParse::Param(FCommandLine::Get(), TEXT("AbilitySmoke"));
}

static void CLTickMove(UCLCombatMovementComponent* Move)
{
	if (Move)
	{
		FActorComponentTickFunction TickFn;
		Move->TickComponent(0.016f, LEVELTICK_All, &TickFn);
	}
}

static void CLRunSlideSmoke(UWorld* World, int32& Failures)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACLPlayerCharacter* Char = World->SpawnActor<ACLPlayerCharacter>(
		ACLVanguardCharacter::StaticClass(), FVector(0.f, -400.f, 220.f), FRotator::ZeroRotator, Params);
	UCLCombatMovementComponent* Move = Char ? Char->GetCombatMovement() : nullptr;
	if (!Move)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: slide pawn missing"));
		++Failures;
		return;
	}

	Move->SetMovementMode(MOVE_Walking);
	Move->SetWantsSprint(false);
	Move->SetWantsCrouch(false);
	Move->Velocity = FVector::ZeroVector;
	CLTickMove(Move);

	Move->SetWantsSprint(true);
	Move->Velocity = FVector::ZeroVector;
	const bool bLatch = Move->TryStartSlide();
	if (!bLatch)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: sprint latch should slide while still slow"));
		++Failures;
	}
	Move->EndSlide();
	Move->SetWantsSprint(false);
	Move->SetWantsCrouch(false);
	CLTickMove(Move);

	Move->Velocity = FVector(200.f, 0.f, 0.f);
	Move->SetWantsSprint(false);
	Move->SetWantsCrouch(true);
	CLTickMove(Move);
	if (Move->IsSliding())
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: crouch-only at walk speed should not slide"));
		++Failures;
	}
	Move->SetWantsCrouch(false);
	CLTickMove(Move);

	Move->Velocity = FVector(Move->GetFullSprintSpeed() + 10.f, 0.f, 0.f);
	Move->SetWantsSprint(false);
	const bool bFallback = Move->TryStartSlide();
	if (!bFallback)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: sprint-speed fallback should slide"));
		++Failures;
	}
	const bool bDodgeOut = Move->TryDodge();
	if (!bDodgeOut)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: dodge-out should work with default ini"));
		++Failures;
	}
	Move->EndSlide();
	Move->SetMovementMode(MOVE_Walking);
	Move->SetWantsSprint(true);
	Move->TryStartSlide();
	Char->Jump();
	if (Move->IsSliding())
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: jump should exit slide"));
		++Failures;
	}

	UE_LOG(LogCalling, Display, TEXT("Calling smoke: slide latch=%d fallback=%d dodgeOut=%d"),
		bLatch ? 1 : 0, bFallback ? 1 : 0, bDodgeOut ? 1 : 0);
}

void CLRunAbilitySmoke(UWorld* World)
{
	if (!World)
	{
		return;
	}

	int32 Failures = 0;
	CLRunSlideSmoke(World, Failures);

	UCLAbilityCatalog* Catalog = UCLAbilityCatalog::Get(World);
	if (!Catalog)
	{
		UE_LOG(LogCalling, Error, TEXT("Calling smoke: catalog missing"));
		++Failures;
		UE_LOG(LogCalling, Display, TEXT("Calling smoke: done failures=%d"), Failures);
		FGenericPlatformMisc::RequestExit(false);
		return;
	}

	struct FCase
	{
		ECLClassId ClassId;
		TSubclassOf<APawn> PawnClass;
		const TCHAR* EvasionId;
		bool bDashMustFailOnGround;
	};
	const FCase Cases[] = {
		{ ECLClassId::Vanguard, ACLVanguardCharacter::StaticClass(), TEXT("fortify"), false },
		{ ECLClassId::Pathfinder, ACLPathfinderCharacter::StaticClass(), TEXT("ripple_camo"), false },
		{ ECLClassId::Warden, ACLWardenCharacter::StaticClass(), TEXT("superposition"), true },
	};

	const FVector Origin(0.f, 0.f, 220.f);
	for (int32 i = 0; i < UE_ARRAY_COUNT(Cases); ++i)
	{
		const FCase& Case = Cases[i];
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACLPlayerCharacter* Char = World->SpawnActor<ACLPlayerCharacter>(
			Case.PawnClass.Get(), Origin + FVector(0.f, i * 200.f, 0.f), FRotator::ZeroRotator, Params);
		if (!Char)
		{
			UE_LOG(LogCalling, Error, TEXT("Calling smoke: failed to spawn class %d"), static_cast<int32>(Case.ClassId));
			++Failures;
			continue;
		}

		FCLCharacterAppearance Appearance;
		Appearance.ClassId = Case.ClassId;
		if (UCLAbilityLoadoutComponent* Loadout = Char->GetAbilities())
		{
			if (!Loadout->LoadFromCharacter(Appearance))
			{
				UE_LOG(LogCalling, Error, TEXT("Calling smoke: loadout failed for class %d"), static_cast<int32>(Case.ClassId));
				++Failures;
			}
			if (UCLAbility* Evasion = Loadout->GetSlot(ECLAbilitySlot::Evasion))
			{
				if (Evasion->GetId() != FName(Case.EvasionId))
				{
					UE_LOG(LogCalling, Error, TEXT("Calling smoke: evasion id %s != %s"), *Evasion->GetId().ToString(), Case.EvasionId);
					++Failures;
				}
			}
		}

		if (UCLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
			Move->Velocity = FVector(400.f, 0.f, 0.f);
			Move->SetMoveInput(FVector2D(0.f, 1.f));
			Move->SetWantsSprint(true);
			const bool bSlid = Move->TryStartSlide();
			const bool bDodged = Move->TryDodge();
			UE_LOG(LogCalling, Display, TEXT("Calling smoke: class %d sprint/slide=%d dodge=%d"),
				static_cast<int32>(Case.ClassId), bSlid ? 1 : 0, bDodged ? 1 : 0);
		}

		Char->Jump();

		UCLAbilityLoadoutComponent* Loadout = Char->GetAbilities();
		const bool bGrenade = Loadout && Loadout->TryGrenade();
		const bool bMelee = Loadout && Loadout->TryMelee();
		const bool bDash = Loadout && Loadout->TryDash();
		const bool bShield = Loadout && Loadout->TryShield();
		const bool bEvasion = Loadout && Loadout->TryEvasion();
		UE_LOG(LogCalling, Display, TEXT("Calling smoke: class %d G=%d M=%d D=%d S=%d E=%d"),
			static_cast<int32>(Case.ClassId), bGrenade, bMelee, bDash, bShield, bEvasion);

		if (!bGrenade || !bMelee || !bShield || !bEvasion)
		{
			++Failures;
		}
		if (Case.bDashMustFailOnGround)
		{
			if (bDash)
			{
				UE_LOG(LogCalling, Error, TEXT("Calling smoke: Warden dash should fail on ground"));
				++Failures;
			}
		}
		else if (!bDash)
		{
			++Failures;
		}

		auto AssertConsumed = [&](ECLAbilitySlot Slot, bool bDidActivate, const TCHAR* Label)
		{
			if (!bDidActivate || !Loadout)
			{
				return;
			}
			const UCLAbility* Ability = Loadout->GetSlot(Slot);
			if (!Ability || Ability->GetCooldownRemaining() <= 0.f)
			{
				UE_LOG(LogCalling, Error, TEXT("Calling smoke: %s did not consume cooldown"), Label);
				++Failures;
				return;
			}
			bool bSecond = false;
			switch (Slot)
			{
			case ECLAbilitySlot::Grenade: bSecond = Loadout->TryGrenade(); break;
			case ECLAbilitySlot::Melee: bSecond = Loadout->TryMelee(); break;
			case ECLAbilitySlot::Dash: bSecond = Loadout->TryDash(); break;
			case ECLAbilitySlot::Shield: bSecond = Loadout->TryShield(); break;
			case ECLAbilitySlot::Evasion: bSecond = Loadout->TryEvasion(); break;
			default: break;
			}
			if (bSecond)
			{
				UE_LOG(LogCalling, Error, TEXT("Calling smoke: %s second press should fail while cooling"), Label);
				++Failures;
			}
		};
		AssertConsumed(ECLAbilitySlot::Grenade, bGrenade, TEXT("G"));
		AssertConsumed(ECLAbilitySlot::Melee, bMelee, TEXT("M"));
		if (!Case.bDashMustFailOnGround)
		{
			AssertConsumed(ECLAbilitySlot::Dash, bDash, TEXT("D"));
		}
		AssertConsumed(ECLAbilitySlot::Shield, bShield, TEXT("S"));
		AssertConsumed(ECLAbilitySlot::Evasion, bEvasion, TEXT("E"));

		if (UCLAbility* JumpAb = Loadout ? Loadout->GetSlot(ECLAbilitySlot::Jump) : nullptr)
		{
			if (JumpAb->Activate(Char))
			{
				UE_LOG(LogCalling, Error, TEXT("Calling smoke: Jump must stay non-activatable"));
				++Failures;
			}
		}

		if (Case.ClassId == ECLClassId::Pathfinder && Char->GetBodyMesh())
		{
			const FVector Scale = Char->GetBodyMesh()->GetComponentScale();
			if (Scale.X > 0.5f)
			{
				UE_LOG(LogCalling, Error, TEXT("Calling smoke: Pathfinder E did not apply ripple camo"));
				++Failures;
			}
		}
	}

	UE_LOG(LogCalling, Display, TEXT("Calling smoke: done failures=%d"), Failures);
	FGenericPlatformMisc::RequestExit(false);
}
