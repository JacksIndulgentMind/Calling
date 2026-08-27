#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DLHeadlessAgent.generated.h"

class UDLPossessionComponent;

/** Seat anchor with no pawn. Can still mind-control. */
UCLASS()
class DESTINYLIKE_API ADLHeadlessAgent : public AActor
{
	GENERATED_BODY()

public:
	ADLHeadlessAgent();

	UDLPossessionComponent* GetPossession() const { return Possession; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "DestinyLike")
	TObjectPtr<UDLPossessionComponent> Possession;
};
