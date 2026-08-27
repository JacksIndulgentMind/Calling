#include "UI/DLSocialMarkerWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UDLSocialMarkerWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		Label->SetText(FText::FromString(TEXT("Social")));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 22;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		if (UCanvasPanelSlot* LabelSlot = Root->AddChildToCanvas(Label))
		{
			LabelSlot->SetAnchors(FAnchors(0.f, 0.f));
			LabelSlot->SetAlignment(FVector2D(0.f, 0.f));
			LabelSlot->SetPosition(FVector2D(24.f, 24.f));
			LabelSlot->SetAutoSize(true);
		}
	}
	return Super::RebuildWidget();
}
