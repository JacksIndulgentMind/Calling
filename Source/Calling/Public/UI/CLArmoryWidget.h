#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Loot/CLLootRulesService.h"
#include "CLArmoryWidget.generated.h"

class UCLArmoryWidget;
class UWidgetTree;
class UVerticalBox;
class UHorizontalBox;
class UComboBoxString;
class UTextBlock;
class UScrollBox;
class UUniformGridPanel;
class UBorder;
class USizeBox;

UCLASS()
class CALLING_API UCLArmoryCellButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UCLArmoryWidget* InOwner, FName InMakeId, int32 InSourceIndex);

	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UCLArmoryWidget> Owner;
	FName MakeId = NAME_None;
	int32 SourceIndex = INDEX_NONE;
	bool bSourceCard = false;
};

/**
 * Obtainability catalog (not vault). Filters, grid, detail, source carousel.
 */
UCLASS()
class CALLING_API UCLArmoryWidget : public UObject
{
	GENERATED_BODY()

public:
	void Build(UWidgetTree* Tree, UVerticalBox* Host);
	void Refresh();
	void SelectMake(FName MakeId);
	void SelectSource(int32 Index);

private:
	UCLLootRulesService* Loot() const;
	void RebuildGrid();
	void RebuildDetail();
	void RebuildSourceStrip();
	void RebuildNestedList(UVerticalBox* Box, const TArray<FString>& Lines, const TCHAR* Empty);

	FName ClassFilter = FName(TEXT("pistol"));
	FName MakerFilter = NAME_None;
	FName SelectedMake = NAME_None;
	int32 SelectedSource = 0;

	TWeakObjectPtr<UWidgetTree> HostTree;

	UPROPERTY()
	TObjectPtr<UComboBoxString> ClassCombo;

	UPROPERTY()
	TObjectPtr<UComboBoxString> MakerCombo;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> Grid;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailName;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailMeta;

	UPROPERTY()
	TObjectPtr<UBorder> ConceptPlate;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatBlock;

	UPROPERTY()
	TObjectPtr<UVerticalBox> ModsBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> PartsBox;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> SourceRow;

	UPROPERTY()
	TObjectPtr<UTextBlock> SourceRate;

	UFUNCTION()
	void HandleClassChanged(FString Selected, ESelectInfo::Type Type);

	UFUNCTION()
	void HandleMakerChanged(FString Selected, ESelectInfo::Type Type);
};
