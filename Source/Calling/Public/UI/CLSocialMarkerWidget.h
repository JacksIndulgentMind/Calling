#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CLSocialMarkerWidget.generated.h"

class UTextBlock;

/** Corner label so a black viewport is distinguishable from "Social loaded." */
UCLASS()
class CALLING_API UCLSocialMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

protected:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
};
