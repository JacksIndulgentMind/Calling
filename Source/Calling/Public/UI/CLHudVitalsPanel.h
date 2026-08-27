#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CLHudVitalsPanel.generated.h"

class ACLPlayerCharacter;
class USizeBox;
class UProgressBar;
class UOverlaySlot;

/** Self and sighted health/shield bars. */
UCLASS()
class CALLING_API UCLHudVitalsPanel : public UObject
{
	GENERATED_BODY()

public:
	void Bind(USizeBox* InSelfVitalsBox, USizeBox* InSelfShieldBox, USizeBox* InSelfHealthBox,
		UProgressBar* InSelfShieldBar, UProgressBar* InSelfHealthBar,
		USizeBox* InSightedBox, UOverlaySlot* InSightedSlot,
		USizeBox* InSightedShieldBox, USizeBox* InSightedHealthBox,
		UProgressBar* InSightedShieldBar, UProgressBar* InSightedHealthBar);

	void ApplyLayout(const FVector2D& ViewportSize, float Inset, float RowW, float Box);
	void RefreshSelf(const ACLPlayerCharacter* Char);
	void RefreshSighted(const ACLPlayerCharacter* Viewer, float DeltaTime);

protected:
	UPROPERTY()
	TObjectPtr<USizeBox> SelfVitalsBox;

	UPROPERTY()
	TObjectPtr<USizeBox> SelfShieldBox;

	UPROPERTY()
	TObjectPtr<USizeBox> SelfHealthBox;

	UPROPERTY()
	TObjectPtr<UProgressBar> SelfShieldBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> SelfHealthBar;

	UPROPERTY()
	TObjectPtr<USizeBox> SightedBox;

	UPROPERTY()
	TObjectPtr<UOverlaySlot> SightedSlot;

	UPROPERTY()
	TObjectPtr<USizeBox> SightedShieldBox;

	UPROPERTY()
	TObjectPtr<USizeBox> SightedHealthBox;

	UPROPERTY()
	TObjectPtr<UProgressBar> SightedShieldBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> SightedHealthBar;

	TWeakObjectPtr<AActor> SightedActor;
	float SightedHealth = 0.f;
	float SightedShield = 0.f;
	float SightedMaxHealth = 100.f;
	float SightedMaxShield = 0.f;
	float SightedSeenAge = 0.f;
	float SightedLostAge = 0.f;
	float SightedAlpha = 0.f;
};
