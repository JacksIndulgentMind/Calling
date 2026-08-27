#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/CLAgentIntent.h"
#include "CLIntentReceiver.generated.h"

/** Latched holdables and one-shot pulses from HTTP, /goto, and playbooks. */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLIntentReceiver : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLIntentReceiver();

	void ApplyAgentIntent(const FCLAgentIntent& Intent);
	void ApplyAgentIntent(FVector2D MoveXY, FVector2D LookDelta, bool bSprint, bool bCrouch, bool bADS, bool bFire,
		bool bJump, bool bDodge, bool bDash, bool bReload, bool bSwap);
	void ClearAgentIntent();
	void ConsumeAgentPulses();

	FVector2D GetMove() const { return AgentMove; }
	FVector2D TakeLookDelta();
	void ClearLookDelta();
	bool WantsSprint() const { return bAgentSprint; }
	bool WantsCrouch() const { return bAgentCrouch; }
	bool WantsADS() const { return bAgentADS; }
	bool WantsFire() const { return bAgentFire; }

protected:
	FVector2D AgentMove = FVector2D::ZeroVector;
	FVector2D AgentLook = FVector2D::ZeroVector;
	bool bAgentSprint = false;
	bool bAgentCrouch = false;
	bool bAgentADS = false;
	bool bAgentFire = false;
	bool bAgentJump = false;
	bool bAgentDodge = false;
	bool bAgentDash = false;
	bool bAgentReload = false;
	bool bAgentSwap = false;
	bool bAgentSlide = false;
	bool bAgentAirDive = false;
	bool bAgentMelee = false;
	bool bAgentWeaponPrimary = false;
	bool bAgentWeaponSpecial = false;
	FName AgentSightId = NAME_None;
};
