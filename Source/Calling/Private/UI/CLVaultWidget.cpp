#include "UI/CLVaultWidget.h"
#include "UI/CLVaultProjection.h"
#include "Game/CLGameInstance.h"
#include "Game/CLLobbySubsystem.h"
#include "Game/CLProfileSubsystem.h"
#include "Game/CLVaultSubsystem.h"
#include "Loot/CLLootRulesService.h"
#include "Player/CLPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UTextBlock* VaultLabel(UWidgetTree* Tree, const FName& Name, const FString& Text, int32 FontSize = 13)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label->SetAutoWrapText(true);
		return Label;
	}

	void AddFill(UHorizontalBox* Box, UWidget* Child, float Fill = 1.f, float PadRight = 8.f)
	{
		if (UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Child))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Fill;
			Slot->SetSize(Size);
			Slot->SetPadding(FMargin(0.f, 0.f, PadRight, 0.f));
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	const TCHAR* RarityName(ECLItemRarity Rarity)
	{
		switch (Rarity)
		{
		case ECLItemRarity::Uncommon: return TEXT("Uncommon");
		case ECLItemRarity::Rare: return TEXT("Rare");
		case ECLItemRarity::Epic: return TEXT("Epic");
		case ECLItemRarity::Legendary: return TEXT("Legendary");
		case ECLItemRarity::Exotic: return TEXT("Exotic");
		default: return TEXT("Common");
		}
	}

	FString StatsLine(const FCLWeaponStats& S)
	{
		return FString::Printf(TEXT("Imp %.0f  Rng %.2f  Stb %.2f  Hnd %.2f"),
			S.Impact, S.Range, S.Stability, S.Handling);
	}

	FString ModsLine(const TArray<FCLModifierRoll>& Mods)
	{
		TArray<FString> Names;
		for (const FCLModifierRoll& M : Mods)
		{
			if (!M.DisplayName.IsEmpty())
			{
				Names.Add(M.DisplayName);
			}
		}
		return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("No mods");
	}
}

void UCLVaultMakeCell::Configure(UCLVaultWidget* InOwner, FName InMakeId)
{
	Owner = InOwner;
	MakeId = InMakeId;
	OnClicked.AddDynamic(this, &UCLVaultMakeCell::HandleClicked);
}

void UCLVaultMakeCell::HandleClicked()
{
	if (Owner.IsValid())
	{
		Owner->SelectMake(MakeId);
	}
}

void UCLVaultEquipButton::Configure(UCLVaultWidget* InOwner, FGuid InInstanceId, bool bInEquippable)
{
	Owner = InOwner;
	InstanceId = InInstanceId;
	bEquippable = bInEquippable;
	OnClicked.AddDynamic(this, &UCLVaultEquipButton::HandleClicked);
}

void UCLVaultEquipButton::HandleClicked()
{
	if (Owner.IsValid())
	{
		Owner->EquipRoll(InstanceId, bEquippable);
	}
}

void UCLVaultWidget::BeginDestroy()
{
	if (UCLVaultSubsystem* Sub = Vault())
	{
		Sub->OnLootEarned.RemoveDynamic(this, &UCLVaultWidget::HandleLootEarned);
	}
	Super::BeginDestroy();
}

UCLLootRulesService* UCLVaultWidget::Loot() const
{
	if (UWorld* World = GetWorld())
	{
		if (UCLGameInstance* GI = Cast<UCLGameInstance>(World->GetGameInstance()))
		{
			return GI->GetLootRulesService();
		}
	}
	return nullptr;
}

UCLVaultSubsystem* UCLVaultWidget::Vault() const
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UCLVaultSubsystem>();
		}
	}
	return nullptr;
}

void UCLVaultWidget::Build(UWidgetTree* Tree, UVerticalBox* Host)
{
	if (!Tree || !Host)
	{
		return;
	}
	HostTree = Tree;

	UHorizontalBox* RowA = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VaultFiltersA"));
	SlotCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("VaultSlot"));
	ClassCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("VaultClass"));
	MakerCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("VaultMaker"));
	AddFill(RowA, VaultLabel(Tree, TEXT("VaultSlotLbl"), TEXT("Slot"), 12), 0.35f, 6.f);
	AddFill(RowA, SlotCombo, 1.f, 10.f);
	AddFill(RowA, VaultLabel(Tree, TEXT("VaultClassLbl"), TEXT("Class"), 12), 0.35f, 6.f);
	AddFill(RowA, ClassCombo, 1.2f, 10.f);
	AddFill(RowA, VaultLabel(Tree, TEXT("VaultMakerLbl"), TEXT("Maker"), 12), 0.4f, 6.f);
	AddFill(RowA, MakerCombo, 1.2f, 0.f);
	if (UVerticalBoxSlot* ASlot = Host->AddChildToVerticalBox(RowA))
	{
		ASlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	UHorizontalBox* RowB = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VaultFiltersB"));
	NameBox = Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("VaultName"));
	NameBox->SetHintText(FText::FromString(TEXT("Name")));
	EquippableCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("VaultEquippable"));
	SortCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("VaultSort"));
	AddFill(RowB, NameBox, 1.4f, 10.f);
	AddFill(RowB, EquippableCombo, 1.f, 10.f);
	AddFill(RowB, SortCombo, 1.1f, 0.f);
	if (UVerticalBoxSlot* BSlot = Host->AddChildToVerticalBox(RowB))
	{
		BSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	ErrorLabel = VaultLabel(Tree, TEXT("VaultError"), TEXT(""), 12);
	ErrorLabel->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ESlot = Host->AddChildToVerticalBox(ErrorLabel))
	{
		ESlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	}

	MakesPane = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VaultMakes"));
	EmptyLabel = VaultLabel(Tree, TEXT("VaultEmpty"), TEXT(""), 14);
	EmptyLabel->SetVisibility(ESlateVisibility::Collapsed);
	MakesPane->AddChildToVerticalBox(EmptyLabel);
	UScrollBox* GridScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("VaultGridScroll"));
	Grid = Tree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("VaultGrid"));
	GridScroll->AddChild(Grid);
	if (UVerticalBoxSlot* GSlot = MakesPane->AddChildToVerticalBox(GridScroll))
	{
		GSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	RollsPane = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VaultRolls"));
	RollsPane->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBox* RollsHead = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("VaultRollsHead"));
	UButton* BackBtn = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("VaultBack"));
	BackBtn->AddChild(VaultLabel(Tree, TEXT("VaultBackLbl"), TEXT("Back"), 14));
	BackBtn->OnClicked.AddDynamic(this, &UCLVaultWidget::HandleBackClicked);
	AddFill(RollsHead, BackBtn, 0.4f, 10.f);
	RollsTitle = VaultLabel(Tree, TEXT("VaultRollsTitle"), TEXT(""), 18);
	AddFill(RollsHead, RollsTitle, 1.6f, 0.f);
	if (UVerticalBoxSlot* HSlot = RollsPane->AddChildToVerticalBox(RollsHead))
	{
		HSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	UScrollBox* RollsScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("VaultRollsScroll"));
	RollsList = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VaultRollsList"));
	RollsScroll->AddChild(RollsList);
	if (UVerticalBoxSlot* RSlot = RollsPane->AddChildToVerticalBox(RollsScroll))
	{
		RSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	if (UVerticalBoxSlot* MSlot = Host->AddChildToVerticalBox(MakesPane))
	{
		MSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UVerticalBoxSlot* PSlot = Host->AddChildToVerticalBox(RollsPane))
	{
		PSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	FillFilterCombos();
	SlotCombo->OnSelectionChanged.AddDynamic(this, &UCLVaultWidget::HandleFilterChanged);
	ClassCombo->OnSelectionChanged.AddDynamic(this, &UCLVaultWidget::HandleFilterChanged);
	MakerCombo->OnSelectionChanged.AddDynamic(this, &UCLVaultWidget::HandleFilterChanged);
	EquippableCombo->OnSelectionChanged.AddDynamic(this, &UCLVaultWidget::HandleFilterChanged);
	SortCombo->OnSelectionChanged.AddDynamic(this, &UCLVaultWidget::HandleFilterChanged);
	NameBox->OnTextChanged.AddDynamic(this, &UCLVaultWidget::HandleNameChanged);

	if (UCLVaultSubsystem* Sub = Vault())
	{
		Sub->OnLootEarned.AddDynamic(this, &UCLVaultWidget::HandleLootEarned);
	}
	Refresh();
}

void UCLVaultWidget::FillFilterCombos()
{
	if (!SlotCombo)
	{
		return;
	}
	bMuteFilterEvents = true;
	SlotCombo->ClearOptions();
	SlotCombo->AddOption(TEXT("All"));
	SlotCombo->AddOption(TEXT("Primary"));
	SlotCombo->AddOption(TEXT("Special"));
	SlotCombo->SetSelectedOption(TEXT("All"));

	ClassCombo->ClearOptions();
	ClassCombo->AddOption(TEXT("All"));
	MakerCombo->ClearOptions();
	MakerCombo->AddOption(TEXT("All"));
	if (UCLLootRulesService* Rules = Loot())
	{
		for (const FCLWeaponClassDef& ClassDef : Rules->GetWeaponClasses())
		{
			ClassCombo->AddOption(ClassDef.Id.ToString());
		}
		for (const FCLWeaponMakerDef& Maker : Rules->GetWeaponMakers())
		{
			MakerCombo->AddOption(Maker.Id.ToString());
		}
	}
	ClassCombo->SetSelectedOption(TEXT("All"));
	MakerCombo->SetSelectedOption(TEXT("All"));

	EquippableCombo->ClearOptions();
	EquippableCombo->AddOption(TEXT("All"));
	EquippableCombo->AddOption(TEXT("This match"));
	EquippableCombo->SetSelectedOption(TEXT("All"));

	SortCombo->ClearOptions();
	SortCombo->AddOption(TEXT("Newest"));
	SortCombo->AddOption(TEXT("Name"));
	SortCombo->AddOption(TEXT("Rarity"));
	SortCombo->AddOption(TEXT("Class"));
	SortCombo->AddOption(TEXT("Equipped first"));
	SortCombo->SetSelectedOption(TEXT("Newest"));
	bMuteFilterEvents = false;
}

FCLVaultFilter UCLVaultWidget::CurrentFilter() const
{
	FCLVaultFilter Filter;
	if (SlotCombo)
	{
		const FString Slot = SlotCombo->GetSelectedOption();
		if (Slot.Equals(TEXT("Primary")))
		{
			Filter.bSlot = true;
			Filter.Slot = ECLWeaponSlot::Primary;
		}
		else if (Slot.Equals(TEXT("Special")))
		{
			Filter.bSlot = true;
			Filter.Slot = ECLWeaponSlot::Special;
		}
	}
	if (ClassCombo)
	{
		const FString Class = ClassCombo->GetSelectedOption();
		if (!Class.IsEmpty() && !Class.Equals(TEXT("All")))
		{
			Filter.ClassId = FName(*Class);
		}
	}
	if (MakerCombo)
	{
		const FString Maker = MakerCombo->GetSelectedOption();
		if (!Maker.IsEmpty() && !Maker.Equals(TEXT("All")))
		{
			Filter.MakerId = FName(*Maker);
		}
	}
	if (NameBox)
	{
		Filter.NameSubstr = NameBox->GetText().ToString();
	}
	if (EquippableCombo)
	{
		Filter.bEquippableOnly = EquippableCombo->GetSelectedOption().Equals(TEXT("This match"));
	}
	return Filter;
}

ECLVaultSort UCLVaultWidget::CurrentSort() const
{
	if (!SortCombo)
	{
		return ECLVaultSort::Newest;
	}
	const FString S = SortCombo->GetSelectedOption();
	if (S.Equals(TEXT("Name"))) return ECLVaultSort::Name;
	if (S.Equals(TEXT("Rarity"))) return ECLVaultSort::Rarity;
	if (S.Equals(TEXT("Class"))) return ECLVaultSort::Class;
	if (S.Equals(TEXT("Equipped first"))) return ECLVaultSort::EquippedFirst;
	return ECLVaultSort::Newest;
}

void UCLVaultWidget::HandleFilterChanged(FString Selected, ESelectInfo::Type Type)
{
	(void)Selected;
	(void)Type;
	if (bMuteFilterEvents)
	{
		return;
	}
	Refresh();
}

void UCLVaultWidget::HandleNameChanged(const FText& Text)
{
	(void)Text;
	if (bMuteFilterEvents)
	{
		return;
	}
	Refresh();
}

void UCLVaultWidget::HandleBackClicked()
{
	TryNavigateBack();
}

void UCLVaultWidget::HandleLootEarned(const FCLItemInstance& Item)
{
	(void)Item;
	if (SelectedMake.IsNone())
	{
		Refresh();
	}
}

void UCLVaultWidget::SetError(const FString& Text)
{
	if (!ErrorLabel)
	{
		return;
	}
	ErrorLabel->SetText(FText::FromString(Text));
	ErrorLabel->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UCLVaultWidget::ResetToMakes()
{
	SelectedMake = NAME_None;
	SetError(TEXT(""));
}

bool UCLVaultWidget::TryNavigateBack()
{
	if (SelectedMake.IsNone())
	{
		return false;
	}
	ResetToMakes();
	Refresh();
	return true;
}

void UCLVaultWidget::SelectMake(FName MakeId)
{
	SelectedMake = MakeId;
	SetError(TEXT(""));
	Refresh();
}

void UCLVaultWidget::EquipRoll(FGuid InstanceId, bool bEquippable)
{
	if (!bEquippable)
	{
		return;
	}
	ACLPlayerController* PC = nullptr;
	if (const UUserWidget* Overlay = GetTypedOuter<UUserWidget>())
	{
		PC = Cast<ACLPlayerController>(Overlay->GetOwningPlayer());
	}
	if (!PC)
	{
		UWorld* World = GetWorld();
		PC = World ? Cast<ACLPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}
	if (!PC || !PC->EquipVaultWeapon(InstanceId))
	{
		SetError(TEXT("Could not equip (see error log)."));
		return;
	}
	SetError(TEXT(""));
	Refresh();
}

void UCLVaultWidget::Refresh()
{
	UCLVaultSubsystem* Sub = Vault();
	UCLLootRulesService* Rules = Loot();
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UCLProfileSubsystem* Profiles = GI ? GI->GetSubsystem<UCLProfileSubsystem>() : nullptr;
	UCLLobbySubsystem* Lobby = GI ? GI->GetSubsystem<UCLLobbySubsystem>() : nullptr;
	if (!Sub || !Profiles)
	{
		return;
	}

	if (ClassCombo && ClassCombo->GetOptionCount() <= 1 && Rules && Rules->GetWeaponClasses().Num() > 0)
	{
		FillFilterCombos();
	}

	const FCLLocalProfile Profile = Profiles->GetActiveProfile();
	const FName LiveRealm = Lobby ? Lobby->GetLootRealmId() : FName(TEXT("local"));
	TArray<FCLVaultMakeGroup> Groups = CLVaultProjection::ProjectWeapons(
		Sub->GetVaultItems(), Rules, Profile.EquippedPrimaryId, Profile.EquippedSpecialId, LiveRealm);
	ProjectedCount = Groups.Num();
	const bool bHadWeapons = ProjectedCount > 0;
	Groups = CLVaultProjection::FilterGroups(Groups, CurrentFilter());
	CLVaultProjection::SortGroups(Groups, CurrentSort());

	if (!SelectedMake.IsNone())
	{
		const FCLVaultMakeGroup* Found = Groups.FindByPredicate([&](const FCLVaultMakeGroup& G)
		{
			return G.DefinitionId == SelectedMake;
		});
		if (Found)
		{
			RebuildRolls(*Found);
			return;
		}
		SelectedMake = NAME_None;
	}
	RebuildMakes(Groups, bHadWeapons);
}

void UCLVaultWidget::RebuildMakes(const TArray<FCLVaultMakeGroup>& Groups, bool bHadWeapons)
{
	if (MakesPane)
	{
		MakesPane->SetVisibility(ESlateVisibility::Visible);
	}
	if (RollsPane)
	{
		RollsPane->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!Grid)
	{
		return;
	}
	Grid->ClearChildren();
	if (EmptyLabel)
	{
		if (Groups.Num() == 0)
		{
			EmptyLabel->SetText(FText::FromString(bHadWeapons
				? TEXT("No makes match filters.")
				: TEXT("No weapons in vault.")));
			EmptyLabel->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			EmptyLabel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	UWidgetTree* Tree = HostTree.Get();
	if (!Tree)
	{
		return;
	}
	const int32 Cols = 3;
	for (int32 i = 0; i < Groups.Num(); ++i)
	{
		const FCLVaultMakeGroup& Group = Groups[i];
		UCLVaultMakeCell* Cell = Tree->ConstructWidget<UCLVaultMakeCell>(UCLVaultMakeCell::StaticClass(),
			*FString::Printf(TEXT("VaultCell_%s"), *Group.DefinitionId.ToString()));
		Cell->Configure(this, Group.DefinitionId);
		if (!Group.bAnyEquippable)
		{
			Cell->SetBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 0.55f));
		}
		FString Title = Group.DisplayName;
		if (Group.bContainsEquipped)
		{
			Title += TEXT("  [E]");
		}
		Title += FString::Printf(TEXT("\n%d rolls"), Group.RollCount);
		UTextBlock* CellLbl = VaultLabel(Tree, *FString::Printf(TEXT("VaultCellLbl_%s"), *Group.DefinitionId.ToString()), Title, 12);
		Cell->AddChild(CellLbl);
		if (UButtonSlot* BSlot = Cast<UButtonSlot>(CellLbl->Slot))
		{
			BSlot->SetPadding(FMargin(8.f));
		}
		if (UUniformGridSlot* GSlot = Grid->AddChildToUniformGrid(Cell, i / Cols, i % Cols))
		{
			GSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UCLVaultWidget::RebuildRolls(const FCLVaultMakeGroup& Group)
{
	if (MakesPane)
	{
		MakesPane->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (RollsPane)
	{
		RollsPane->SetVisibility(ESlateVisibility::Visible);
	}
	if (RollsTitle)
	{
		RollsTitle->SetText(FText::FromString(Group.DisplayName));
	}
	if (!RollsList)
	{
		return;
	}
	RollsList->ClearChildren();
	UWidgetTree* Tree = HostTree.Get();
	if (!Tree)
	{
		return;
	}

	TArray<FCLVaultRollRow> Rolls = Group.Rolls;
	CLVaultProjection::SortRolls(Rolls, CurrentSort());
	for (int32 i = 0; i < Rolls.Num(); ++i)
	{
		const FCLVaultRollRow& Row = Rolls[i];
		UHorizontalBox* Line = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("VaultRoll_%d"), i));
		FString Body = FString::Printf(TEXT("%s%s\n%s\n%s"),
			RarityName(Row.Rarity),
			Row.bEquipped ? TEXT("  [E]") : TEXT(""),
			*StatsLine(Row.FinalStats),
			*ModsLine(Row.Modifiers));
		AddFill(Line, VaultLabel(Tree, *FString::Printf(TEXT("VaultRollLbl_%d"), i), Body, 12), 1.8f, 8.f);

		UCLVaultEquipButton* Equip = Tree->ConstructWidget<UCLVaultEquipButton>(UCLVaultEquipButton::StaticClass(),
			*FString::Printf(TEXT("VaultEquip_%d"), i));
		Equip->Configure(this, Row.InstanceId, Row.bEquippableThisMatch);
		Equip->AddChild(VaultLabel(Tree, *FString::Printf(TEXT("VaultEquipLbl_%d"), i),
			Row.bEquippableThisMatch ? TEXT("Equip") : TEXT("Wrong realm"), 12));
		if (!Row.bEquippableThisMatch)
		{
			Equip->SetIsEnabled(false);
			Equip->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.5f));
		}
		AddFill(Line, Equip, 0.55f, 0.f);
		if (UVerticalBoxSlot* LSlot = RollsList->AddChildToVerticalBox(Line))
		{
			LSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
	}
}
