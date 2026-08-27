#include "Game/DLAbilitySmoke.h"
#include "Game/DLGameModeBase.h"
#include "Core/DLLog.h"
#include "Game/DLGameStateBase.h"
#include "Game/DLActivityStateComponent.h"
#include "Game/DLSceneRouter.h"
#include "Game/DLVaultSubsystem.h"
#include "Game/DLProfileSubsystem.h"
#include "Game/DLGameInstance.h"
#include "Game/DLSessionSubsystem.h"
#include "Loot/DLLootRulesService.h"
#include "AI/DLEncounterDirector.h"
#include "AI/DLPracticeDummy.h"
#include "Player/DLPlayerCharacter.h"
#include "Player/DLHealthShieldComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Player/DLPlayerController.h"
#include "Player/DLVanguardCharacter.h"
#include "Player/DLPathfinderCharacter.h"
#include "Player/DLWardenCharacter.h"
#include "Player/DLCombatMovementComponent.h"
#include "Player/DLAbilityLoadoutComponent.h"
#include "Ability/DLAbilityCatalog.h"
#include "Ability/DLAbility.h"
#include "HAL/IConsoleManager.h"
#include "UI/DLBootProfileWidget.h"
#include "UI/DLSocialMarkerWidget.h"
#include "Game/DLGreyboxFloors.h"
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

bool DLShouldRunAbilitySmoke()
{
	return CVarDLAbilitySmoke.GetValueOnGameThread() > 0 || FParse::Param(FCommandLine::Get(), TEXT("AbilitySmoke"));
}

static void DLTickMove(UDLCombatMovementComponent* Move)
{
	if (Move)
	{
		FActorComponentTickFunction TickFn;
		Move->TickComponent(0.016f, LEVELTICK_All, &TickFn);
	}
}

static void DLRunSlideSmoke(UWorld* World, int32& Failures)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADLPlayerCharacter* Char = World->SpawnActor<ADLPlayerCharacter>(
		ADLVanguardCharacter::StaticClass(), FVector(0.f, -400.f, 220.f), FRotator::ZeroRotator, Params);
	UDLCombatMovementComponent* Move = Char ? Char->GetCombatMovement() : nullptr;
	if (!Move)
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: slide pawn missing"));
		++Failures;
		return;
	}

	Move->SetMovementMode(MOVE_Walking);
	Move->SetWantsSprint(false);
	Move->SetWantsCrouch(false);
	Move->Velocity = FVector::ZeroVector;
	DLTickMove(Move);

	Move->SetWantsSprint(true);
	Move->Velocity = FVector::ZeroVector;
	const bool bLatch = Move->TryStartSlide();
	if (!bLatch)
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: sprint latch should slide while still slow"));
		++Failures;
	}
	Move->EndSlide();
	Move->SetWantsSprint(false);
	Move->SetWantsCrouch(false);
	DLTickMove(Move);

	Move->Velocity = FVector(200.f, 0.f, 0.f);
	Move->SetWantsSprint(false);
	Move->SetWantsCrouch(true);
	DLTickMove(Move);
	if (Move->IsSliding())
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: crouch-only at walk speed should not slide"));
		++Failures;
	}
	Move->SetWantsCrouch(false);
	DLTickMove(Move);

	Move->Velocity = FVector(Move->GetFullSprintSpeed() + 10.f, 0.f, 0.f);
	Move->SetWantsSprint(false);
	const bool bFallback = Move->TryStartSlide();
	if (!bFallback)
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: sprint-speed fallback should slide"));
		++Failures;
	}
	const bool bDodgeOut = Move->TryDodge();
	if (!bDodgeOut)
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: dodge-out should work with default ini"));
		++Failures;
	}
	Move->EndSlide();
	Move->SetMovementMode(MOVE_Walking);
	Move->SetWantsSprint(true);
	Move->TryStartSlide();
	Char->Jump();
	if (Move->IsSliding())
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: jump should exit slide"));
		++Failures;
	}

	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike smoke: slide latch=%d fallback=%d dodgeOut=%d"),
		bLatch ? 1 : 0, bFallback ? 1 : 0, bDodgeOut ? 1 : 0);
}

void DLRunAbilitySmoke(UWorld* World)
{
	if (!World)
	{
		return;
	}

	int32 Failures = 0;
	DLRunSlideSmoke(World, Failures);

	UDLAbilityCatalog* Catalog = UDLAbilityCatalog::Get(World);
	if (!Catalog)
	{
		UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: catalog missing"));
		++Failures;
		UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike smoke: done failures=%d"), Failures);
		FGenericPlatformMisc::RequestExit(false);
		return;
	}

	struct FCase
	{
		EDLClassId ClassId;
		TSubclassOf<APawn> PawnClass;
		const TCHAR* EvasionId;
		bool bDashMustFailOnGround;
	};
	const FCase Cases[] = {
		{ EDLClassId::Vanguard, ADLVanguardCharacter::StaticClass(), TEXT("fortify"), false },
		{ EDLClassId::Pathfinder, ADLPathfinderCharacter::StaticClass(), TEXT("ripple_camo"), false },
		{ EDLClassId::Warden, ADLWardenCharacter::StaticClass(), TEXT("superposition"), true },
	};

	const FVector Origin(0.f, 0.f, 220.f);
	for (int32 i = 0; i < UE_ARRAY_COUNT(Cases); ++i)
	{
		const FCase& Case = Cases[i];
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADLPlayerCharacter* Char = World->SpawnActor<ADLPlayerCharacter>(
			Case.PawnClass.Get(), Origin + FVector(0.f, i * 200.f, 0.f), FRotator::ZeroRotator, Params);
		if (!Char)
		{
			UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: failed to spawn class %d"), static_cast<int32>(Case.ClassId));
			++Failures;
			continue;
		}

		FDLCharacterAppearance Appearance;
		Appearance.ClassId = Case.ClassId;
		if (UDLAbilityLoadoutComponent* Loadout = Char->GetAbilities())
		{
			if (!Loadout->LoadFromCharacter(Appearance))
			{
				UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: loadout failed for class %d"), static_cast<int32>(Case.ClassId));
				++Failures;
			}
			if (UDLAbility* Evasion = Loadout->GetSlot(EDLAbilitySlot::Evasion))
			{
				if (Evasion->GetId() != FName(Case.EvasionId))
				{
					UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: evasion id %s != %s"), *Evasion->GetId().ToString(), Case.EvasionId);
					++Failures;
				}
			}
		}

		if (UDLCombatMovementComponent* Move = Char->GetCombatMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
			Move->Velocity = FVector(400.f, 0.f, 0.f);
			Move->SetMoveInput(FVector2D(0.f, 1.f));
			Move->SetWantsSprint(true);
			const bool bSlid = Move->TryStartSlide();
			const bool bDodged = Move->TryDodge();
			UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike smoke: class %d sprint/slide=%d dodge=%d"),
				static_cast<int32>(Case.ClassId), bSlid ? 1 : 0, bDodged ? 1 : 0);
		}

		Char->Jump();

		UDLAbilityLoadoutComponent* Loadout = Char->GetAbilities();
		const bool bGrenade = Loadout && Loadout->TryGrenade();
		const bool bMelee = Loadout && Loadout->TryMelee();
		const bool bDash = Loadout && Loadout->TryDash();
		const bool bShield = Loadout && Loadout->TryShield();
		const bool bEvasion = Loadout && Loadout->TryEvasion();
		UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike smoke: class %d G=%d M=%d D=%d S=%d E=%d"),
			static_cast<int32>(Case.ClassId), bGrenade, bMelee, bDash, bShield, bEvasion);

		if (!bGrenade || !bMelee || !bShield || !bEvasion)
		{
			++Failures;
		}
		if (Case.bDashMustFailOnGround)
		{
			if (bDash)
			{
				UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: Warden dash should fail on ground"));
				++Failures;
			}
		}
		else if (!bDash)
		{
			++Failures;
		}

		auto AssertConsumed = [&](EDLAbilitySlot Slot, bool bDidActivate, const TCHAR* Label)
		{
			if (!bDidActivate || !Loadout)
			{
				return;
			}
			const UDLAbility* Ability = Loadout->GetSlot(Slot);
			if (!Ability || Ability->GetCooldownRemaining() <= 0.f)
			{
				UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: %s did not consume cooldown"), Label);
				++Failures;
				return;
			}
			bool bSecond = false;
			switch (Slot)
			{
			case EDLAbilitySlot::Grenade: bSecond = Loadout->TryGrenade(); break;
			case EDLAbilitySlot::Melee: bSecond = Loadout->TryMelee(); break;
			case EDLAbilitySlot::Dash: bSecond = Loadout->TryDash(); break;
			case EDLAbilitySlot::Shield: bSecond = Loadout->TryShield(); break;
			case EDLAbilitySlot::Evasion: bSecond = Loadout->TryEvasion(); break;
			default: break;
			}
			if (bSecond)
			{
				UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: %s second press should fail while cooling"), Label);
				++Failures;
			}
		};
		AssertConsumed(EDLAbilitySlot::Grenade, bGrenade, TEXT("G"));
		AssertConsumed(EDLAbilitySlot::Melee, bMelee, TEXT("M"));
		if (!Case.bDashMustFailOnGround)
		{
			AssertConsumed(EDLAbilitySlot::Dash, bDash, TEXT("D"));
		}
		AssertConsumed(EDLAbilitySlot::Shield, bShield, TEXT("S"));
		AssertConsumed(EDLAbilitySlot::Evasion, bEvasion, TEXT("E"));

		if (UDLAbility* JumpAb = Loadout ? Loadout->GetSlot(EDLAbilitySlot::Jump) : nullptr)
		{
			if (JumpAb->Activate(Char))
			{
				UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: Jump must stay non-activatable"));
				++Failures;
			}
		}

		if (Case.ClassId == EDLClassId::Pathfinder && Char->GetBodyMesh())
		{
			const FVector Scale = Char->GetBodyMesh()->GetComponentScale();
			if (Scale.X > 0.5f)
			{
				UE_LOG(LogDestinyLike, Error, TEXT("DestinyLike smoke: Pathfinder E did not apply ripple camo"));
				++Failures;
			}
		}
	}

	UE_LOG(LogDestinyLike, Display, TEXT("DestinyLike smoke: done failures=%d"), Failures);
	FGenericPlatformMisc::RequestExit(false);
}
