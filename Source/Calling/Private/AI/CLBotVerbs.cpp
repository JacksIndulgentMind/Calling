#include "AI/CLBotVerbs.h"
#include "AI/CLTaskMarker.h"
#include "Game/CLSeatMotor.h"
#include "Game/CLParticipantSeat.h"
#include "Game/CLLobbySubsystem.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Nav/CLNavAbilityExec.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLCombatMovementComponent.h"
#include "Player/CLAbilityLoadoutComponent.h"
#include "Player/CLWeaponMotorComponent.h"
#include "Ability/CLAbilityTypes.h"
#include "Input/CLAgentIntent.h"
#include "Engine/World.h"

namespace
{
	FVector2D MoveFromLeaf(const FCLBotLeaf& Leaf)
	{
		if (Leaf.Move.Contains(TEXT("strafe")))
		{
			return FVector2D(1.f, 0.f);
		}
		if (Leaf.Move.Contains(TEXT("back")))
		{
			return FVector2D(0.f, -1.f);
		}
		if (Leaf.Move.Contains(TEXT("still")) || Leaf.Move.Contains(TEXT("stop")))
		{
			return FVector2D::ZeroVector;
		}
		return FVector2D(0.f, 1.f);
	}

	void PulseIntent(ACLPlayerCharacter* Char, const FCLAgentIntent& Intent)
	{
		if (Char)
		{
			Char->ApplyAgentIntent(Intent);
		}
	}

	FGuid ResolveFocusSeat(FCLBotVerbContext& Ctx)
	{
		if (Ctx.Leaf)
		{
			const FString* SeatStr = Ctx.Leaf->Params.Find(TEXT("seat"));
			if (SeatStr)
			{
				FGuid Id;
				if (FGuid::Parse(*SeatStr, Id))
				{
					return Id;
				}
			}
		}
		return Ctx.FocusSeat;
	}
}

void CLApplyBotWhile(FCLBotVerbContext& Ctx, bool bIncludeMove)
{
	if (!Ctx.Char || !Ctx.Leaf)
	{
		return;
	}
	FCLAgentIntent Intent;
	if (bIncludeMove)
	{
		Intent.Move = MoveFromLeaf(*Ctx.Leaf);
		Intent.bSprint = true;
	}
	for (const FName& W : Ctx.Leaf->WhileVerbs)
	{
		const FString S = W.ToString().ToLower();
		if (S == TEXT("trackfocus") || S == TEXT("setfocus"))
		{
			Ctx.Char->SetLookTrackSeat(ResolveFocusSeat(Ctx));
		}
		else if (S == TEXT("maintainads"))
		{
			Intent.bADS = true;
		}
		else if (S == TEXT("fire"))
		{
			Intent.bFire = true;
		}
	}
	if (bIncludeMove)
	{
		PulseIntent(Ctx.Char, Intent);
	}
	else if (Intent.bADS || Intent.bFire)
	{
		Ctx.Char->LatchAgentWhileHolds(Intent.bADS, Intent.bFire);
	}
}

namespace
{
	struct FGotoVerb final : ICLBotVerb
	{
		bool bStarted = false;
		bool bFailedStart = false;

		void Start(FCLBotVerbContext& Ctx) override
		{
			bFailedStart = false;
			if (!Ctx.Motor || !Ctx.Char || !Ctx.Leaf)
			{
				bFailedStart = true;
				return;
			}
			FString Err;
			if (!Ctx.Motor->StartGoto(Ctx.World, Ctx.Char, Ctx.Goal, Err))
			{
				bFailedStart = true;
			}
			bStarted = true;
		}

		void Tick(float DeltaSeconds, FCLBotVerbContext& Ctx) override
		{
			(void)DeltaSeconds;
			if (!Ctx.Motor || !Ctx.Char)
			{
				return;
			}
			if (Ctx.Motor->GetGotoDriver().bActive)
			{
				Ctx.Motor->GetGotoDriver().Tick(DeltaSeconds, Ctx.World, Ctx.Char);
			}
			CLApplyBotWhile(Ctx, false);
		}

		void Stop(FCLBotVerbContext& Ctx) override
		{
			if (Ctx.Motor)
			{
				Ctx.Motor->CancelGoto();
			}
			if (Ctx.Char)
			{
				Ctx.Char->ClearAgentIntent();
			}
		}

		bool SuccessImpossible(const FCLBotVerbContext& Ctx) const override
		{
			if (bFailedStart)
			{
				return true;
			}
			if (!Ctx.Motor)
			{
				return true;
			}
			return bStarted && !Ctx.Motor->IsGotoActive();
		}
	};

	struct FPulseVerb final : ICLBotVerb
	{
		FName Kind;
		int32 Fired = 0;
		float Acc = 0.f;
		FCLNavAbilityExec Exec;
		bool bTo = false;

		explicit FPulseVerb(FName InKind) : Kind(InKind) {}

		bool IsToLeaf(const FCLBotVerbContext& Ctx) const
		{
			if (!Ctx.Leaf)
			{
				return false;
			}
			const FString K = Kind.ToString().ToLower();
			if (K != TEXT("airdive") && K != TEXT("jump") && K != TEXT("slide")
				&& K != TEXT("dash") && K != TEXT("dodge"))
			{
				return false;
			}
			return Ctx.Leaf->Params.Contains(TEXT("marker")) || Ctx.Leaf->Params.Contains(TEXT("x"));
		}

		ECLNavAbilityExecMode ModeForKind() const
		{
			const FString K = Kind.ToString().ToLower();
			if (K == TEXT("jump")) { return ECLNavAbilityExecMode::JumpTo; }
			if (K == TEXT("slide")) { return ECLNavAbilityExecMode::SlideTo; }
			if (K == TEXT("dash")) { return ECLNavAbilityExecMode::DashTo; }
			if (K == TEXT("dodge")) { return ECLNavAbilityExecMode::DodgeTo; }
			if (K == TEXT("airdive")) { return ECLNavAbilityExecMode::AirDiveTo; }
			return ECLNavAbilityExecMode::Launch;
		}

		void Start(FCLBotVerbContext& Ctx) override
		{
			Fired = 0;
			Acc = 0.f;
			bTo = IsToLeaf(Ctx);
			if (bTo && Ctx.Char)
			{
				if (Ctx.Motor)
				{
					Ctx.Motor->CancelGoto();
				}
				Exec.Mode = ModeForKind();
				Exec.Goal = Ctx.Goal;
				const int32 LeafPulses = Ctx.Leaf ? Ctx.Leaf->Pulses : 1;
				Exec.JumpPulses = Exec.Mode == ECLNavAbilityExecMode::Launch
					? FMath::Max(5, LeafPulses)
					: FMath::Max(1, LeafPulses);
				Exec.PulseGap = Ctx.Leaf ? Ctx.Leaf->PulseGap : 0.12f;
				Exec.LandRadius = 180.f;
				if (Ctx.Leaf && Ctx.Leaf->Success.Name.Equals(TEXT("distXY"), ESearchCase::IgnoreCase))
				{
					Exec.LandRadius = FCString::Atof(*Ctx.Leaf->Success.Value);
				}
				Exec.Start(Ctx.Char);
				return;
			}
			Fire(Ctx);
		}

		void Tick(float DeltaSeconds, FCLBotVerbContext& Ctx) override
		{
			if (bTo)
			{
				if (Ctx.Leaf && (Ctx.Leaf->Params.Contains(TEXT("marker")) || Ctx.Leaf->Params.Contains(TEXT("x"))))
				{
					Exec.Goal = Ctx.Goal;
				}
				Exec.Tick(DeltaSeconds, Ctx.Char);
				CLApplyBotWhile(Ctx, false);
				return;
			}
			Acc += DeltaSeconds;
			const int32 Want = Ctx.Leaf ? FMath::Max(1, Ctx.Leaf->Pulses) : 1;
			const float Gap = Ctx.Leaf ? Ctx.Leaf->PulseGap : 0.12f;
			if (Fired < Want && Acc >= Gap)
			{
				Acc = 0.f;
				Fire(Ctx);
			}
			else
			{
				Hold(Ctx);
			}
			CLApplyBotWhile(Ctx, false);
		}

		void Stop(FCLBotVerbContext& Ctx) override
		{
			if (Ctx.Char)
			{
				Ctx.Char->ClearAgentIntent();
			}
		}

		bool InsideSuccessBand(const FCLBotVerbContext& Ctx) const
		{
			if (!Ctx.Char || !Ctx.Leaf || Ctx.Leaf->Success.IsEmpty())
			{
				return false;
			}
			if (!Ctx.Leaf->Success.Name.Equals(TEXT("distXY"), ESearchCase::IgnoreCase))
			{
				return false;
			}
			const float D = FVector::Dist2D(Ctx.Char->GetActorLocation(), Ctx.Goal);
			const float Rhs = FCString::Atof(*Ctx.Leaf->Success.Value);
			return D <= Rhs;
		}

		FVector2D StickForPulse(const FCLBotVerbContext& Ctx) const
		{
			if (InsideSuccessBand(Ctx))
			{
				return FVector2D::ZeroVector;
			}
			if (Ctx.Char && Ctx.Leaf && (Ctx.Leaf->Params.Contains(TEXT("marker")) || Ctx.Leaf->Params.Contains(TEXT("x")))
				&& !Ctx.Goal.IsNearlyZero())
			{
				const FVector To = (Ctx.Goal - Ctx.Char->GetActorLocation()).GetSafeNormal2D();
				if (!To.IsNearlyZero())
				{
					Ctx.Char->SetLookGoalYawPitch(true, To.Rotation().Yaw, true, 0.f);
					return FVector2D(0.f, 1.f);
				}
			}
			return Ctx.Leaf ? MoveFromLeaf(*Ctx.Leaf) : FVector2D::ZeroVector;
		}

		bool SuccessImpossible(const FCLBotVerbContext& Ctx) const override
		{
			if (bTo)
			{
				return Exec.bFailed || Exec.SuccessImpossible(Ctx.Char);
			}
			return false;
		}

		void Fire(FCLBotVerbContext& Ctx)
		{
			++Fired;
			if (!Ctx.Char)
			{
				return;
			}
			FCLAgentIntent Intent;
			Intent.Move = StickForPulse(Ctx);
			Intent.bSprint = !Intent.Move.IsNearlyZero();
			const FString K = Kind.ToString().ToLower();
			if (K == TEXT("jump")) { Intent.bJump = true; }
			else if (K == TEXT("slide")) { Intent.bSlide = true; }
			else if (K == TEXT("airdive")) { Intent.bAirDive = true; }
			else if (K == TEXT("dodge")) { Intent.bDodge = true; }
			else if (K == TEXT("dash")) { Intent.bDash = true; }
			else if (K == TEXT("melee")) { Intent.bMelee = true; }
			else if (K == TEXT("fire")) { Intent.bFire = true; }
			PulseIntent(Ctx.Char, Intent);
		}

		void Hold(FCLBotVerbContext& Ctx)
		{
			if (!Ctx.Char || !Ctx.Leaf)
			{
				return;
			}
			const FString K = Kind.ToString().ToLower();
			FCLAgentIntent Intent;
			if (K == TEXT("fire"))
			{
				Intent.bFire = true;
				PulseIntent(Ctx.Char, Intent);
				return;
			}
			Intent.Move = StickForPulse(Ctx);
			Intent.bSprint = !Intent.Move.IsNearlyZero();
			if (K == TEXT("airdive")) { Intent.bAirDive = true; }
			PulseIntent(Ctx.Char, Intent);
		}
	};

	struct FFocusVerb final : ICLBotVerb
	{
		void Start(FCLBotVerbContext& Ctx) override
		{
			if (Ctx.Char)
			{
				Ctx.Char->SetLookTrackSeat(ResolveFocusSeat(Ctx));
			}
		}
		void Tick(float, FCLBotVerbContext& Ctx) override
		{
			if (Ctx.Char)
			{
				Ctx.Char->SetLookTrackSeat(ResolveFocusSeat(Ctx));
			}
			CLApplyBotWhile(Ctx, false);
		}
		void Stop(FCLBotVerbContext& Ctx) override { (void)Ctx; }
	};

	struct FAdsVerb final : ICLBotVerb
	{
		void Start(FCLBotVerbContext& Ctx) override { Tick(0.f, Ctx); }
		void Tick(float, FCLBotVerbContext& Ctx) override
		{
			if (Ctx.Char)
			{
				FCLAgentIntent Intent;
				Intent.bADS = true;
				PulseIntent(Ctx.Char, Intent);
			}
		}
		void Stop(FCLBotVerbContext& Ctx) override { (void)Ctx; }
	};

	struct FAbilityVerb final : ICLBotVerb
	{
		bool bFocus = false;
		bool bFired = false;
		explicit FAbilityVerb(bool bInFocus) : bFocus(bInFocus) {}

		void Start(FCLBotVerbContext& Ctx) override
		{
			bFired = false;
			if (bFocus)
			{
				const FGuid Focus = ResolveFocusSeat(Ctx);
				if (Ctx.Char && Focus.IsValid())
				{
					Ctx.Char->SetLookTrackSeat(Focus);
				}
			}
			Fire(Ctx);
		}
		void Tick(float, FCLBotVerbContext& Ctx) override
		{
			if (!bFired)
			{
				Fire(Ctx);
			}
		}
		void Stop(FCLBotVerbContext& Ctx) override { (void)Ctx; }

		void Fire(FCLBotVerbContext& Ctx)
		{
			if (!Ctx.Char || !Ctx.Leaf)
			{
				return;
			}
			UCLAbilityLoadoutComponent* Abs = Ctx.Char->GetAbilities();
			if (!Abs)
			{
				return;
			}
			FString SlotName = TEXT("melee");
			if (const FString* Id = Ctx.Leaf->Params.Find(TEXT("slot")))
			{
				SlotName = *Id;
			}
			else if (const FString* Ability = Ctx.Leaf->Params.Find(TEXT("ability")))
			{
				SlotName = *Ability;
			}
			ECLAbilitySlot Slot;
			if (!CLParseAbilitySlot(SlotName, Slot))
			{
				return;
			}
			switch (Slot)
			{
			case ECLAbilitySlot::Grenade: Abs->TryGrenade(); break;
			case ECLAbilitySlot::Shield: Abs->TryShield(); break;
			case ECLAbilitySlot::Evasion: Abs->TryEvasion(); break;
			case ECLAbilitySlot::Dash: Abs->TryDash(); break;
			case ECLAbilitySlot::Melee: Abs->TryMelee(); break;
			case ECLAbilitySlot::Super: Abs->TrySuper(); break;
			default: break;
			}
			bFired = true;
		}
	};

	struct FIdleVerb final : ICLBotVerb
	{
		void Start(FCLBotVerbContext& Ctx) override { (void)Ctx; }
		void Tick(float, FCLBotVerbContext& Ctx) override
		{
			if (Ctx.Char)
			{
				FCLAgentIntent Intent;
				PulseIntent(Ctx.Char, Intent);
			}
		}
		void Stop(FCLBotVerbContext& Ctx) override { (void)Ctx; }
	};
}

TUniquePtr<ICLBotVerb> CLMakeBotVerb(FName VerbId)
{
	const FString S = VerbId.ToString().ToLower();
	if (S == TEXT("goto")) { return MakeUnique<FGotoVerb>(); }
	if (S == TEXT("setfocus") || S == TEXT("trackfocus")) { return MakeUnique<FFocusVerb>(); }
	if (S == TEXT("maintainads")) { return MakeUnique<FAdsVerb>(); }
	if (S == TEXT("useabilityself")) { return MakeUnique<FAbilityVerb>(false); }
	if (S == TEXT("useabilityfocus")) { return MakeUnique<FAbilityVerb>(true); }
	if (S == TEXT("jump") || S == TEXT("slide") || S == TEXT("airdive") || S == TEXT("dodge")
		|| S == TEXT("dash") || S == TEXT("melee") || S == TEXT("fire"))
	{
		return MakeUnique<FPulseVerb>(FName(*S));
	}
	if (S == TEXT("wait") || S == TEXT("idle")) { return MakeUnique<FIdleVerb>(); }
	return nullptr;
}
