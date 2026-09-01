#include "UI/CLBootProfileWidget.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLSceneRouter.h"
#include "Game/CLSessionSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName& Name, const FString& Text, int32 FontSize = 14)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return Label;
	}

	USizeBox* WrapHeight(UWidgetTree* Tree, const FName& Name, UWidget* Child, float Height)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		Box->SetHeightOverride(Height);
		Box->AddChild(Child);
		return Box;
	}

	void AddChildPadded(UVerticalBox* Box, UWidget* Child, float BottomPad = 8.f)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPad));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
}

TSharedRef<SWidget> UCLBootProfileWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UCLBootProfileWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 1.f));
	Dim->SetPadding(FMargin(0.f));
	if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimSlot->SetOffsets(FMargin(0.f));
	}

	USizeBox* FormSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FormSize"));
	FormSize->SetWidthOverride(460.f);
	if (UCanvasPanelSlot* FormSlot = RootCanvas->AddChildToCanvas(FormSize))
	{
		FormSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FormSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FormSlot->SetAutoSize(true);
		FormSlot->SetPosition(FVector2D::ZeroVector);
	}

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FormPanel"));
	Panel->SetBrushColor(FLinearColor(0.06f, 0.07f, 0.1f, 0.96f));
	Panel->SetPadding(FMargin(28.f, 24.f));
	FormSize->AddChild(Panel);

	FormBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FormBox"));
	Panel->AddChild(FormBox);

	TitleText = MakeLabel(WidgetTree, TEXT("TitleText"), TEXT("Create Player"), 28);
	AddChildPadded(FormBox, TitleText, 16.f);

	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("ExistingLabel"), TEXT("Existing profiles")), 4.f);
	ExistingProfileCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ExistingProfileCombo"));
	ExistingProfileCombo->OnSelectionChanged.AddDynamic(this, &UCLBootProfileWidget::HandleExistingProfileChanged);
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("ExistingComboSize"), ExistingProfileCombo, 36.f), 12.f);

	ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueLabel"));
		Label->SetText(FText::FromString(TEXT("Continue Selected")));
		ContinueButton->AddChild(Label);
		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(Label->Slot))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Center);
			BtnSlot->SetVerticalAlignment(VAlign_Center);
		}
		ContinueButton->OnClicked.AddDynamic(this, &UCLBootProfileWidget::HandleContinueClicked);
	}
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("ContinueSize"), ContinueButton, 40.f), 20.f);

	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("NewSection"), TEXT("— or create new —")), 12.f);

	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("ProfileNameLabel"), TEXT("Profile name")), 4.f);
	ProfileNameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("ProfileNameBox"));
	ProfileNameBox->SetHintText(FText::FromString(TEXT("Player")));
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("ProfileNameSize"), ProfileNameBox, 36.f));

	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("CharNameLabel"), TEXT("Character name")), 4.f);
	CharacterNameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("CharacterNameBox"));
	CharacterNameBox->SetHintText(FText::FromString(TEXT("Ash")));
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("CharNameSize"), CharacterNameBox, 36.f));

	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("ClassLabel"), TEXT("Class")), 4.f);
	ClassCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ClassCombo"));
	PopulateClassCombo();
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("ClassComboSize"), ClassCombo, 36.f));

	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("SexLabel"), TEXT("Appearance")), 4.f);
	SexCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("SexCombo"));
	PopulateSexCombo();
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("SexComboSize"), SexCombo, 36.f));

	DefaultProfileCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("DefaultProfileCheck"));
	DefaultProfileCheck->SetIsChecked(true);
	AddChildPadded(FormBox, MakeLabel(WidgetTree, TEXT("DefaultLabel"), TEXT("Set as default profile")), 4.f);
	AddChildPadded(FormBox, DefaultProfileCheck, 16.f);

	CreateEnterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CreateEnterButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CreateEnterLabel"));
		Label->SetText(FText::FromString(TEXT("Create & Enter Social")));
		CreateEnterButton->AddChild(Label);
		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(Label->Slot))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Center);
			BtnSlot->SetVerticalAlignment(VAlign_Center);
		}
		CreateEnterButton->OnClicked.AddDynamic(this, &UCLBootProfileWidget::HandleCreateAndEnterClicked);
	}
	AddChildPadded(FormBox, WrapHeight(WidgetTree, TEXT("CreateEnterSize"), CreateEnterButton, 40.f), 12.f);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::GetEmpty());
	AddChildPadded(FormBox, StatusText, 0.f);
}

void UCLBootProfileWidget::PopulateClassCombo()
{
	if (!ClassCombo) return;
	ClassCombo->ClearOptions();
	ClassCombo->AddOption(TEXT("Vanguard"));
	ClassCombo->AddOption(TEXT("Pathfinder"));
	ClassCombo->AddOption(TEXT("Warden"));
	ClassCombo->SetSelectedOption(TEXT("Vanguard"));
}

void UCLBootProfileWidget::PopulateSexCombo()
{
	if (!SexCombo) return;
	SexCombo->ClearOptions();
	SexCombo->AddOption(TEXT("Male"));
	SexCombo->AddOption(TEXT("Female"));
	SexCombo->AddOption(TEXT("Other"));
	SexCombo->SetSelectedOption(TEXT("Other"));
}

void UCLBootProfileWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	RefreshExistingProfiles();

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
}

void UCLBootProfileWidget::RefreshExistingProfiles()
{
	ExistingProfileIds.Reset();
	if (!ExistingProfileCombo)
	{
		return;
	}

	ExistingProfileCombo->ClearOptions();
	ExistingProfileCombo->AddOption(TEXT("(new profile)"));

	UCLProfileSubsystem* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLProfileSubsystem>() : nullptr;
	if (!Profiles)
	{
		ExistingProfileCombo->SetSelectedOption(TEXT("(new profile)"));
		return;
	}

	Profiles->LoadAllProfiles();
	for (const FCLLocalProfile& P : Profiles->GetAllProfiles())
	{
		ExistingProfileIds.Add(P.ProfileId);
		const FString Label = FString::Printf(
			TEXT("%s%s — %s%s"),
			*P.DisplayName,
			P.bIsDefault ? TEXT(" [default]") : TEXT(""),
			P.Character.CharacterName.IsEmpty() ? TEXT("(no character)") : *P.Character.CharacterName,
			P.Character.bLockedIn ? TEXT("") : TEXT(" [incomplete]"));
		ExistingProfileCombo->AddOption(Label);
	}
	ExistingProfileCombo->SetSelectedIndex(0);
}

void UCLBootProfileWidget::HandleExistingProfileChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	(void)SelectedItem;
	(void)SelectionType;
}

void UCLBootProfileWidget::SetStatus(const FString& Message, bool bError)
{
	if (!StatusText) return;
	StatusText->SetText(FText::FromString(Message));
	StatusText->SetColorAndOpacity(bError ? FSlateColor(FLinearColor(1.f, 0.35f, 0.35f)) : FSlateColor(FLinearColor(0.6f, 1.f, 0.6f)));
}

void UCLBootProfileWidget::EnterSocial()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UCLSessionSubsystem* Sessions = GI->GetSubsystem<UCLSessionSubsystem>())
		{
			Sessions->ApplySocialDefault();
			return;
		}
		if (UCLSceneRouter* Router = GI->GetSubsystem<UCLSceneRouter>())
		{
			Router->TravelToScene(ECLSceneId::Social);
		}
	}
}

void UCLBootProfileWidget::HandleCreateAndEnterClicked()
{
	if (CommitNewProfileAndEnter())
	{
		SetStatus(TEXT("Entering Social…"), false);
		EnterSocial();
	}
}

void UCLBootProfileWidget::HandleContinueClicked()
{
	if (ContinueExistingAndEnter())
	{
		SetStatus(TEXT("Entering Social…"), false);
		EnterSocial();
	}
}

bool UCLBootProfileWidget::ContinueExistingAndEnter()
{
	UCLProfileSubsystem* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLProfileSubsystem>() : nullptr;
	if (!Profiles || !ExistingProfileCombo)
	{
		SetStatus(TEXT("Profile system unavailable."), true);
		return false;
	}

	const int32 Index = ExistingProfileCombo->GetSelectedIndex();
	// 0 == "(new profile)"
	if (Index <= 0)
	{
		SetStatus(TEXT("Select an existing profile, or use Create & Enter."), true);
		return false;
	}

	const int32 ProfileIndex = Index - 1;
	if (!ExistingProfileIds.IsValidIndex(ProfileIndex))
	{
		SetStatus(TEXT("Invalid profile selection."), true);
		return false;
	}

	const FGuid Id = ExistingProfileIds[ProfileIndex];
	if (!Profiles->SelectProfile(Id))
	{
		SetStatus(TEXT("Could not select profile."), true);
		return false;
	}

	const FCLLocalProfile Active = Profiles->GetActiveProfile();
	if (!Active.Character.bLockedIn)
	{
		SetStatus(TEXT("That profile has no locked-in character. Create a new one or finish via Create."), true);
		return false;
	}

	return true;
}

bool UCLBootProfileWidget::CommitNewProfileAndEnter()
{
	UCLProfileSubsystem* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCLProfileSubsystem>() : nullptr;
	if (!Profiles)
	{
		SetStatus(TEXT("Profile system unavailable."), true);
		return false;
	}

	FString ProfileName = ProfileNameBox ? ProfileNameBox->GetText().ToString().TrimStartAndEnd() : FString();
	FString CharacterName = CharacterNameBox ? CharacterNameBox->GetText().ToString().TrimStartAndEnd() : FString();
	if (ProfileName.IsEmpty())
	{
		ProfileName = TEXT("Player");
	}
	if (CharacterName.IsEmpty())
	{
		SetStatus(TEXT("Enter a character name."), true);
		return false;
	}

	ECLClassId ClassId = ECLClassId::Vanguard;
	if (ClassCombo)
	{
		const FString ClassStr = ClassCombo->GetSelectedOption();
		if (ClassStr.Equals(TEXT("Pathfinder"), ESearchCase::IgnoreCase)) ClassId = ECLClassId::Pathfinder;
		else if (ClassStr.Equals(TEXT("Warden"), ESearchCase::IgnoreCase)) ClassId = ECLClassId::Warden;
	}

	ECLCharacterSex Sex = ECLCharacterSex::Other;
	if (SexCombo)
	{
		const FString SexStr = SexCombo->GetSelectedOption();
		if (SexStr.Equals(TEXT("Male"), ESearchCase::IgnoreCase)) Sex = ECLCharacterSex::Male;
		else if (SexStr.Equals(TEXT("Female"), ESearchCase::IgnoreCase)) Sex = ECLCharacterSex::Female;
	}

	FCLLocalProfile Profile = Profiles->CreateProfile(ProfileName);

	FCLCharacterAppearance Appearance;
	Appearance.ClassId = ClassId;
	Appearance.Sex = Sex;
	Appearance.LookId = FName(*FString::Printf(TEXT("look_%d"), static_cast<int32>(Sex)));
	Appearance.CharacterName = CharacterName;
	Appearance.bLockedIn = false;

	if (!Profiles->LockInCharacter(Profile.ProfileId, Appearance))
	{
		SetStatus(TEXT("Failed to lock in character."), true);
		return false;
	}

	const bool bDefault = !DefaultProfileCheck || DefaultProfileCheck->IsChecked();
	Profiles->SetDefaultProfile(Profile.ProfileId, bDefault);
	Profiles->SelectProfile(Profile.ProfileId);
	return true;
}
