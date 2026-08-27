#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DLTypes.h"
#include "DLBootProfileWidget.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UEditableTextBox;
class UComboBoxString;
class UButton;
class UTextBlock;
class UCheckBox;

/**
 * Minimal C++-built boot UI: create/select profile, lock in character, enter Social.
 * No Widget Blueprint required.
 */
UCLASS()
class DESTINYLIKE_API UDLBootProfileWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "DestinyLike|Boot")
	void RefreshExistingProfiles();

protected:
	void BuildWidgetTree();
	void PopulateClassCombo();
	void PopulateSexCombo();

	UFUNCTION()
	void HandleCreateAndEnterClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleExistingProfileChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	bool CommitNewProfileAndEnter();
	bool ContinueExistingAndEnter();
	void SetStatus(const FString& Message, bool bError);
	void EnterSocial();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UVerticalBox> FormBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UComboBoxString> ExistingProfileCombo;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> ProfileNameBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> CharacterNameBox;

	UPROPERTY()
	TObjectPtr<UComboBoxString> ClassCombo;

	UPROPERTY()
	TObjectPtr<UComboBoxString> SexCombo;

	UPROPERTY()
	TObjectPtr<UCheckBox> DefaultProfileCheck;

	UPROPERTY()
	TObjectPtr<UButton> CreateEnterButton;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY()
	TArray<FGuid> ExistingProfileIds;
};
