#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "CLHudPainter.generated.h"

class ACLPlayerCharacter;
struct FCLRadarPaintBlip;

/** Slate overlays: hurt vignette, sight reticle, radar disc. */
UCLASS()
class CALLING_API UCLHudPainter : public UObject
{
	GENERATED_BODY()

public:
	void PaintSightCrosshair(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const ACLPlayerCharacter* Char) const;
	void PaintHurtVignette(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const ACLPlayerCharacter* Char) const;
	void PaintRadar(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const TArray<FCLRadarPaintBlip>& Blips, const float Wedge[3], bool bHasCharacter) const;
};
