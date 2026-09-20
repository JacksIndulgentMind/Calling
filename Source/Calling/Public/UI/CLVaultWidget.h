#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Game/CLVaultSubsystem.h"
#include "UI/CLVaultProjection.h"
#include "CLVaultWidget.generated.h"

class UCLVaultWidget;
class UWidgetTree;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UScrollBox;
class UUniformGridPanel;
class UEditableTextBox;
class UCLLootRulesService;

UCLASS()
class CALLING_API UCLVaultMakeCell : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UCLVaultWidget* InOwner, FName InMakeId);

	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UCLVaultWidget> Owner;
	FName MakeId = NAME_None;
};

UCLASS()
class CALLING_API UCLVaultEquipButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UCLVaultWidget* InOwner, FGuid InInstanceId, bool bEquippable);

	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UCLVaultWidget> Owner;
	FGuid InstanceId;
	bool bEquippable = false;
};

UCLASS()
class CALLING_API UCLVaultWidget : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void Build(UWidgetTree* Tree, UVerticalBox* Host);
	void Refresh();
	void SelectMake(FName MakeId);
	void EquipRoll(FGuid InstanceId, bool bEquippable);
	bool TryNavigateBack();
	void ResetToMakes();

private:
	UCLLootRulesService* Loot() const;
	UCLVaultSubsystem* Vault() const;
	FCLVaultFilter CurrentFilter() const;
	ECLVaultSort CurrentSort() const;
	void RebuildMakes(const TArray<FCLVaultMakeGroup>& Groups, bool bHadWeapons);
	void RebuildRolls(const FCLVaultMakeGroup& Group);
	void FillFilterCombos();
	void SetError(const FString& Text);

	FName SelectedMake = NAME_None;
	int32 ProjectedCount = 0;
	bool bMuteFilterEvents = false;

	TWeakObjectPtr<UWidgetTree> HostTree;

	UPROPERTY()
	TObjectPtr<UComboBoxString> SlotCombo;
	UPROPERTY()
	TObjectPtr<UComboBoxString> ClassCombo;
	UPROPERTY()
	TObjectPtr<UComboBoxString> MakerCombo;
	UPROPERTY()
	TObjectPtr<UComboBoxString> EquippableCombo;
	UPROPERTY()
	TObjectPtr<UComboBoxString> SortCombo;
	UPROPERTY()
	TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY()
	TObjectPtr<UVerticalBox> MakesPane;
	UPROPERTY()
	TObjectPtr<UVerticalBox> RollsPane;
	UPROPERTY()
	TObjectPtr<UUniformGridPanel> Grid;
	UPROPERTY()
	TObjectPtr<UTextBlock> EmptyLabel;
	UPROPERTY()
	TObjectPtr<UTextBlock> RollsTitle;
	UPROPERTY()
	TObjectPtr<UVerticalBox> RollsList;
	UPROPERTY()
	TObjectPtr<UTextBlock> ErrorLabel;

	UFUNCTION()
	void HandleFilterChanged(FString Selected, ESelectInfo::Type Type);
	UFUNCTION()
	void HandleNameChanged(const FText& Text);
	UFUNCTION()
	void HandleBackClicked();
	UFUNCTION()
	void HandleLootEarned(const FCLItemInstance& Item);
};
