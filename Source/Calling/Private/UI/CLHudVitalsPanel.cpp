#include "UI/CLHudVitalsPanel.h"
#include "Player/CLPlayerCharacter.h"
#include "Player/CLHealthShieldComponent.h"
#include "Combat/CLHitscanService.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"

void UCLHudVitalsPanel::Bind(USizeBox* InSelfVitalsBox, USizeBox* InSelfShieldBox, USizeBox* InSelfHealthBox,
	UProgressBar* InSelfShieldBar, UProgressBar* InSelfHealthBar,
	USizeBox* InSightedBox, UOverlaySlot* InSightedSlot,
	USizeBox* InSightedShieldBox, USizeBox* InSightedHealthBox,
	UProgressBar* InSightedShieldBar, UProgressBar* InSightedHealthBar)
{
	SelfVitalsBox = InSelfVitalsBox;
	SelfShieldBox = InSelfShieldBox;
	SelfHealthBox = InSelfHealthBox;
	SelfShieldBar = InSelfShieldBar;
	SelfHealthBar = InSelfHealthBar;
	SightedBox = InSightedBox;
	SightedSlot = InSightedSlot;
	SightedShieldBox = InSightedShieldBox;
	SightedHealthBox = InSightedHealthBox;
	SightedShieldBar = InSightedShieldBar;
	SightedHealthBar = InSightedHealthBar;
}

void UCLHudVitalsPanel::ApplyLayout(const FVector2D& ViewportSize, float Inset, float RowW, float Box)
{
	const float Short = FMath::Max(1.f, FMath::Min(ViewportSize.X, ViewportSize.Y));
	const float SelfBarH = FMath::Max(5.f, Box * 0.14f);
	if (SelfVitalsBox)
	{
		SelfVitalsBox->SetWidthOverride(RowW);
		SelfVitalsBox->SetHeightOverride(SelfBarH * 2.f + 3.f);
	}
	if (SelfShieldBox)
	{
		SelfShieldBox->SetHeightOverride(SelfBarH);
	}
	if (SelfHealthBox)
	{
		SelfHealthBox->SetHeightOverride(SelfBarH);
	}
	const float SightW = Short * 0.16f;
	const float SightBarH = SelfBarH;
	if (SightedBox)
	{
		SightedBox->SetWidthOverride(SightW);
		SightedBox->SetHeightOverride(SightBarH * 2.f + 2.f);
	}
	if (SightedShieldBox)
	{
		SightedShieldBox->SetHeightOverride(SightBarH);
	}
	if (SightedHealthBox)
	{
		SightedHealthBox->SetHeightOverride(SightBarH);
	}
	if (SightedSlot)
	{
		SightedSlot->SetPadding(FMargin(0.f, 0.f, Inset, Inset));
	}
}

void UCLHudVitalsPanel::RefreshSelf(const ACLPlayerCharacter* Char)
{
	const UCLHealthShieldComponent* HS = Char ? Char->GetHealthShield() : nullptr;
	if (SelfVitalsBox)
	{
		SelfVitalsBox->SetVisibility(HS ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!HS)
	{
		return;
	}
	const float MaxS = FMath::Max(1.f, HS->GetTune().MaxShield);
	const float MaxH = FMath::Max(1.f, HS->GetTune().MaxHealth);
	if (SelfShieldBar)
	{
		SelfShieldBar->SetPercent(FMath::Clamp(HS->GetShield() / MaxS, 0.f, 1.f));
	}
	if (SelfHealthBar)
	{
		SelfHealthBar->SetPercent(FMath::Clamp(HS->GetHealth() / MaxH, 0.f, 1.f));
	}
}

void UCLHudVitalsPanel::RefreshSighted(const ACLPlayerCharacter* Viewer, float DeltaTime)
{
	constexpr float FadeIn = 0.18f;
	constexpr float FadeOut = 0.40f;
	FCLSightedTarget Target;
	const bool bHit = CLHitscanService::QuerySightedFromPawn(Viewer, Target);
	if (bHit)
	{
		if (SightedActor.Get() != Target.Actor.Get())
		{
			SightedSeenAge = 0.f;
		}
		SightedActor = Target.Actor;
		SightedHealth = Target.Health;
		SightedShield = Target.Shield;
		SightedMaxHealth = Target.MaxHealth;
		SightedMaxShield = Target.MaxShield;
		SightedLostAge = 0.f;
		SightedSeenAge += DeltaTime;
		SightedAlpha = FMath::Clamp(SightedSeenAge / FadeIn, 0.f, 1.f);
	}
	else
	{
		SightedSeenAge = 0.f;
		SightedLostAge += DeltaTime;
		SightedAlpha = 1.f - FMath::Clamp(SightedLostAge / FadeOut, 0.f, 1.f);
		if (SightedLostAge >= FadeOut)
		{
			SightedActor.Reset();
			SightedAlpha = 0.f;
		}
	}

	if (!SightedBox)
	{
		return;
	}
	if (SightedAlpha <= 0.01f)
	{
		SightedBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SightedBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	SightedBox->SetRenderOpacity(SightedAlpha);
	if (SightedHealthBar)
	{
		SightedHealthBar->SetPercent(FMath::Clamp(SightedHealth / FMath::Max(1.f, SightedMaxHealth), 0.f, 1.f));
	}
	const bool bHasShield = SightedMaxShield > 0.5f;
	if (SightedShieldBox)
	{
		SightedShieldBox->SetVisibility(bHasShield ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SightedShieldBar && bHasShield)
	{
		SightedShieldBar->SetPercent(FMath::Clamp(SightedShield / FMath::Max(1.f, SightedMaxShield), 0.f, 1.f));
	}
}
