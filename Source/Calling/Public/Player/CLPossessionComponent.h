#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/CLLobbyTypes.h"
#include "CLPossessionComponent.generated.h"

/** Sole input owner for a seat: own pawn, mind-control, or headless. */
UCLASS(ClassGroup = (Calling), meta = (BlueprintSpawnableComponent))
class CALLING_API UCLPossessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCLPossessionComponent();

	UFUNCTION(BlueprintPure, Category = "Calling|Possession")
	ECLPossessionMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category = "Calling|Possession")
	APawn* GetDrivenPawn() const;

	UFUNCTION(BlueprintCallable, Category = "Calling|Possession")
	void PossessOwn(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "Calling|Possession")
	void MindControl(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "Calling|Possession")
	void GoHeadless();

	UFUNCTION(BlueprintPure, Category = "Calling|Possession")
	bool Drives(const APawn* Pawn) const;

protected:
	UPROPERTY()
	ECLPossessionMode Mode = ECLPossessionMode::Headless;

	UPROPERTY()
	TWeakObjectPtr<APawn> DrivenPawn;

	UPROPERTY()
	TWeakObjectPtr<APawn> OwnPawn;
};
