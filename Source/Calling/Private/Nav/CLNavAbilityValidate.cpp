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
	if (!Near(Move.BaseStrafeSpeed, 380.f, 0.02f) || !Near(Move.AirControl, 0.35f, 0.02f))
	{
		R.bLocksOk = false;
		R.bApplyAirDiveLink = false;
		R.Message = FString::Printf(TEXT("locked feel drifted strafe=%.1f airControl=%.3f"), Move.BaseStrafeSpeed, Move.AirControl);
		return R;
	}

	const float SingleApex = CLNavAbility::JumpApexUpCm(Move, 1);
	if (Nav.JumpApexCm + 1.f < SingleApex * 0.95f)
	{
		R.bApplyAirDiveLink = false;
		R.Message = FString::Printf(TEXT("jumpApexCm %.0f < single jump apex %.0f"), Nav.JumpApexCm, SingleApex);
		return R;
	}

	if (Nav.AirDiveSearchMaxCm > 0.f)
	{
		const float PhysMax = CLNavAbility::MaxLaunchXY(Move, CLNavAbility::AirDiveRefDropCm());
		if (Nav.AirDiveSearchMaxCm > PhysMax * 1.05f)
		{
			R.bApplyAirDiveLink = false;
			R.Message = FString::Printf(TEXT("airDiveSearchMaxCm %.0f > MaxLaunchXY %.0f"), Nav.AirDiveSearchMaxCm, PhysMax);
			return R;
		}
	}

	const float PhysLen = CLNavAbility::MaxLaunchXY(Move, CLNavAbility::AirDiveRefDropCm());
	const float BakedLen = CLNavAbility::SearchRadiusCm(Move, Nav, CLNavAbility::AirDiveRefDropCm());
	if (!Near(BakedLen, PhysLen, 0.05f) && Nav.AirDiveSearchMaxCm <= 0.f)
	{
		R.bApplyAirDiveLink = false;
		R.Message = FString::Printf(TEXT("AirDive JumpLength %.0f != MaxLaunchXY %.0f at %.0f drop"), BakedLen, PhysLen, CLNavAbility::AirDiveRefDropCm());
		return R;
	}
	float AirDiveEdgeCm = 40.f;
	for (const FCLNavLinkTune& L : Nav.Links)
	{
		if (!CLNavTune::IsAirDiveLink(L.Name))
		{
			continue;
		}
		AirDiveEdgeCm = L.JumpDistanceFromEdge;
		if (L.JumpLength > PhysLen * 1.05f)
		{
			R.bApplyAirDiveLink = false;
			R.Message = FString::Printf(TEXT("%s json jumpLength %.0f > MaxLaunchXY %.0f"), *L.Name.ToString(), L.JumpLength, PhysLen);
			return R;
		}
	}
	const float DownLen = CLNavAbility::AirDiveBakeJumpLengthCm(Move, Nav, AirDiveEdgeCm);
	if (DownLen > PhysLen * 1.05f)
	{
		R.bApplyAirDiveLink = false;
		R.Message = FString::Printf(TEXT("AirDiveDown bake JumpLength %.0f > MaxLaunchXY %.0f"), DownLen, PhysLen);
		return R;
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
