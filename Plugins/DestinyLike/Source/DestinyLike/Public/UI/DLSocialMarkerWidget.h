#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DLSocialMarkerWidget.generated.h"

class UTextBlock;

/** Corner label so a black viewport is distinguishable from "Social loaded." */
UCLASS()
class DESTINYLIKE_API UDLSocialMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

protected:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;
};
