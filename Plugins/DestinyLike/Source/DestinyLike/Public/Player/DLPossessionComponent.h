#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/DLLobbyTypes.h"
#include "DLPossessionComponent.generated.h"

/** Sole input owner for a seat: own pawn, mind-control, or headless. */
UCLASS(ClassGroup = (DestinyLike), meta = (BlueprintSpawnableComponent))
class DESTINYLIKE_API UDLPossessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDLPossessionComponent();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Possession")
	EDLPossessionMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Possession")
	APawn* GetDrivenPawn() const;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Possession")
	void PossessOwn(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Possession")
	void MindControl(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Possession")
	void GoHeadless();

	UFUNCTION(BlueprintPure, Category = "DestinyLike|Possession")
	bool Drives(const APawn* Pawn) const;

protected:
	UPROPERTY()
	EDLPossessionMode Mode = EDLPossessionMode::Headless;

	UPROPERTY()
	TWeakObjectPtr<APawn> DrivenPawn;

	UPROPERTY()
	TWeakObjectPtr<APawn> OwnPawn;
};
