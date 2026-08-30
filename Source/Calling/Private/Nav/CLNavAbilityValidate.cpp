#include "Nav/CLNavAbilityValidate.h"
#include "Nav/CLNavAbilityEnvelope.h"
#include "Nav/CLNavTune.h"
#include "Core/CLTunes.h"
#include "Core/CLError.h"
#include "Core/CLLog.h"
#include "Game/CLErrorBoundary.h"

namespace
{
	bool Near(float A, float B, float Tol)
	{
		const float Scale = FMath::Max(1.f, FMath::Max(FMath::Abs(A), FMath::Abs(B)));
		return FMath::Abs(A - B) <= Tol * Scale;
	}
}

FCLNavAbilityValidateResult CLNavAbilityValidate::Check(const FCLMovementTune& Move, const FCLNavTune& Nav, float SurvivingDropCm)
{
	FCLNavAbilityValidateResult R;
	(void)SurvivingDropCm;
	// Feel locks only. Do not disable AirDive because JumpLength / search / place chord
	// exceeds MaxLaunchXY — those are not JumpLength ceilings.
	if (!Near(Move.BaseStrafeSpeed, 380.f, 0.02f) || !Near(Move.AirControl, 0.35f, 0.02f))
	{
		R.bLocksOk = false;
		R.bApplyAirDiveLink = false;
		R.Message = FString::Printf(TEXT("locked feel drifted strafe=%.1f airControl=%.3f"), Move.BaseStrafeSpeed, Move.AirControl);
		return R;
	}

	const float TripleApex = CLNavAbility::JumpApexUpCm(Move, FMath::Max(1, Move.MaxJumps));
	if (Nav.JumpApexCm + 1.f < TripleApex * 0.95f)
	{
		R.bApplyAirDiveLink = false;
		R.Message = FString::Printf(TEXT("jumpApexCm %.0f < triple jump apex %.0f"), Nav.JumpApexCm, TripleApex);
		return R;
	}

	for (const FCLNavLinkTune& L : Nav.Links)
	{
		if (!CLNavTune::IsAirDiveLink(L.Name))
		{
			continue;
		}
		const float Height = CLNavTune::ResolveScalar(L.JumpHeight, TripleApex, Nav, SurvivingDropCm);
		if (Height + 1.f < TripleApex * 0.95f)
		{
			R.bApplyAirDiveLink = false;
			R.Message = FString::Printf(TEXT("%s JumpHeight %.0f < triple apex %.0f"),
				*L.Name.ToString(), Height, TripleApex);
			return R;
		}
	}

	return R;
}

void CLNavAbilityValidate::Report(UObject* WorldContext, const FCLNavAbilityValidateResult& Result)
{
	if (Result.Message.IsEmpty())
	{
		return;
	}
	UE_LOG(LogCalling, Warning, TEXT("NavAbilityValidate: %s"), *Result.Message);
	UCLErrorBoundary::ReportStatic(WorldContext, FCLError::Make(
		ECLErrorKind::Logic,
		TEXT("nav_ability_validate"),
		Result.Message));
}
