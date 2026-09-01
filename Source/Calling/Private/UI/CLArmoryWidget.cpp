#include "UI/CLArmoryWidget.h"
#include "Game/CLGameInstance.h"
#include "Loot/CLLootRulesService.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UTextBlock* ArmoryLabel(UWidgetTree* Tree, const FName& Name, const FString& Text, int32 FontSize = 13)
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

	FLinearColor ClassTint(FName ClassId)
	{
		if (ClassId == FName(TEXT("pistol"))) return FLinearColor(0.35f, 0.22f, 0.12f, 1.f);
		if (ClassId == FName(TEXT("smg"))) return FLinearColor(0.18f, 0.28f, 0.22f, 1.f);
		if (ClassId == FName(TEXT("rifle"))) return FLinearColor(0.16f, 0.22f, 0.32f, 1.f);
		if (ClassId == FName(TEXT("dmr"))) return FLinearColor(0.22f, 0.18f, 0.30f, 1.f);
		if (ClassId == FName(TEXT("shotgun"))) return FLinearColor(0.32f, 0.16f, 0.16f, 1.f);
		if (ClassId == FName(TEXT("sniper_rifle"))) return FLinearColor(0.12f, 0.16f, 0.24f, 1.f);
		if (ClassId == FName(TEXT("grenade_rifle"))) return FLinearColor(0.28f, 0.24f, 0.10f, 1.f);
		return FLinearColor(0.20f, 0.18f, 0.14f, 1.f);
	}

	const TCHAR* KindLabel(ECLDropSourceKind Kind)
	{
		switch (Kind)
		{
		case ECLDropSourceKind::RaidBoss: return TEXT("Raid boss");
		case ECLDropSourceKind::RaidMob: return TEXT("Raid mob");
		case ECLDropSourceKind::PvpAward: return TEXT("PvP award");
		case ECLDropSourceKind::PvpComplete: return TEXT("PvP");
		case ECLDropSourceKind::FactionVendor: return TEXT("Vendor");
		default: return TEXT("World");
		}
	}
}

void UCLArmoryCellButton::Configure(UCLArmoryWidget* InOwner, FName InMakeId, int32 InSourceIndex)
{
	Owner = InOwner;
	MakeId = InMakeId;
	SourceIndex = InSourceIndex;
	bSourceCard = InSourceIndex != INDEX_NONE;
	OnClicked.AddDynamic(this, &UCLArmoryCellButton::HandleClicked);
}

void UCLArmoryCellButton::HandleClicked()
{
	if (!Owner.IsValid())
	{
		return;
	}
	if (bSourceCard)
	{
		Owner->SelectSource(SourceIndex);
	}
	else
	{
		Owner->SelectMake(MakeId);
	}
}

UCLLootRulesService* UCLArmoryWidget::Loot() const
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

void UCLArmoryWidget::Build(UWidgetTree* Tree, UVerticalBox* Host)
{
	if (!Tree || !Host)
	{
		return;
	}
	HostTree = Tree;

	UHorizontalBox* Filters = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ArmoryFilters"));
	ClassCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ArmoryClass"));
	MakerCombo = Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("ArmoryMaker"));
	AddFill(Filters, ArmoryLabel(Tree, TEXT("ClassLbl"), TEXT("Class"), 12), 0.35f, 6.f);
	AddFill(Filters, ClassCombo, 1.2f, 12.f);
	AddFill(Filters, ArmoryLabel(Tree, TEXT("MakerLbl"), TEXT("Make"), 12), 0.35f, 6.f);
	AddFill(Filters, MakerCombo, 1.2f, 0.f);
	if (UVerticalBoxSlot* FSlot = Host->AddChildToVerticalBox(Filters))
	{
		FSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	UHorizontalBox* Panes = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ArmoryPanes"));

	UVerticalBox* DetailCol = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ArmoryDetail"));
	DetailName = ArmoryLabel(Tree, TEXT("ArmoryName"), TEXT(""), 20);
	DetailMeta = ArmoryLabel(Tree, TEXT("ArmoryMeta"), TEXT(""), 13);
	ConceptPlate = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ArmoryConcept"));
	ConceptPlate->SetBrushColor(FLinearColor(0.12f, 0.10f, 0.08f, 1.f));
	USizeBox* ConceptSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ArmoryConceptSize"));
	ConceptSize->SetHeightOverride(160.f);
	ConceptSize->AddChild(ConceptPlate);
	StatBlock = ArmoryLabel(Tree, TEXT("ArmoryStats"), TEXT(""), 12);
	ModsBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ArmoryMods"));
	PartsBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ArmoryParts"));
	SourceRate = ArmoryLabel(Tree, TEXT("ArmoryRate"), TEXT(""), 12);
	UScrollBox* SourceScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ArmorySources"));
	SourceScroll->SetOrientation(Orient_Horizontal);
	SourceRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ArmorySourceRow"));
	SourceScroll->AddChild(SourceRow);
	USizeBox* SourceSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ArmorySourceSize"));
	SourceSize->SetHeightOverride(72.f);
	SourceSize->AddChild(SourceScroll);

	auto AddDetail = [&](UWidget* Child, float Bottom)
	{
		if (UVerticalBoxSlot* Slot = DetailCol->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	};
	AddDetail(DetailName, 4.f);
	AddDetail(DetailMeta, 8.f);
	AddDetail(ConceptSize, 8.f);
	AddDetail(ArmoryLabel(Tree, TEXT("SrcHint"), TEXT("Sources"), 12), 4.f);
	AddDetail(SourceSize, 4.f);
	AddDetail(SourceRate, 8.f);
	AddDetail(ArmoryLabel(Tree, TEXT("StatHint"), TEXT("Stat band (selected source)"), 12), 4.f);
	AddDetail(StatBlock, 8.f);
	AddDetail(ArmoryLabel(Tree, TEXT("ModHint"), TEXT("Mods"), 12), 4.f);
	AddDetail(ModsBox, 8.f);
	AddDetail(ArmoryLabel(Tree, TEXT("PartHint"), TEXT("Extensions"), 12), 4.f);
	AddDetail(PartsBox, 0.f);

	UScrollBox* DetailScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ArmoryDetailScroll"));
	DetailScroll->AddChild(DetailCol);

	UScrollBox* GridScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ArmoryGridScroll"));
	Grid = Tree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ArmoryGrid"));
	GridScroll->AddChild(Grid);

	AddFill(Panes, DetailScroll, 1.15f, 16.f);
	AddFill(Panes, GridScroll, 1.f, 0.f);
	if (UVerticalBoxSlot* PSlot = Host->AddChildToVerticalBox(Panes))
	{
		PSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PSlot->SetHorizontalAlignment(HAlign_Fill);
		PSlot->SetVerticalAlignment(VAlign_Fill);
	}

	if (UCLLootRulesService* Rules = Loot())
	{
		ClassCombo->ClearOptions();
		for (const FCLWeaponClassDef& ClassDef : Rules->GetWeaponClasses())
		{
			ClassCombo->AddOption(ClassDef.Id.ToString());
		}
		ClassCombo->SetSelectedOption(TEXT("pistol"));
		MakerCombo->ClearOptions();
		MakerCombo->AddOption(TEXT("All"));
		for (const FCLWeaponMakerDef& Maker : Rules->GetWeaponMakers())
		{
			MakerCombo->AddOption(Maker.Id.ToString());
		}
		MakerCombo->SetSelectedOption(TEXT("All"));
	}
	ClassCombo->OnSelectionChanged.AddDynamic(this, &UCLArmoryWidget::HandleClassChanged);
	MakerCombo->OnSelectionChanged.AddDynamic(this, &UCLArmoryWidget::HandleMakerChanged);
	Refresh();
}

void UCLArmoryWidget::HandleClassChanged(FString Selected, ESelectInfo::Type Type)
{
	(void)Type;
	ClassFilter = Selected.IsEmpty() ? FName(TEXT("pistol")) : FName(*Selected);
	Refresh();
}

void UCLArmoryWidget::HandleMakerChanged(FString Selected, ESelectInfo::Type Type)
{
	(void)Type;
	MakerFilter = (Selected.IsEmpty() || Selected.Equals(TEXT("All"))) ? NAME_None : FName(*Selected);
	Refresh();
}

void UCLArmoryWidget::Refresh()
{
	UCLLootRulesService* Rules = Loot();
	if (Rules && ClassCombo && ClassCombo->GetOptionCount() == 0)
	{
		for (const FCLWeaponClassDef& ClassDef : Rules->GetWeaponClasses())
		{
			ClassCombo->AddOption(ClassDef.Id.ToString());
		}
		ClassCombo->SetSelectedOption(TEXT("pistol"));
		if (MakerCombo)
		{
			MakerCombo->AddOption(TEXT("All"));
			for (const FCLWeaponMakerDef& Maker : Rules->GetWeaponMakers())
			{
				MakerCombo->AddOption(Maker.Id.ToString());
			}
			MakerCombo->SetSelectedOption(TEXT("All"));
		}
	}
	const TArray<FCLWeaponMakeDef> Matches = Rules
		? Rules->MakesMatching(ClassFilter, MakerFilter)
		: TArray<FCLWeaponMakeDef>();
	if (Matches.Num() == 0)
	{
		SelectedMake = NAME_None;
	}
	else
	{
		bool bKeep = false;
		for (const FCLWeaponMakeDef& M : Matches)
		{
			if (M.Id == SelectedMake)
			{
				bKeep = true;
				break;
			}
		}
		if (!bKeep)
		{
			SelectedMake = Matches[0].Id;
			SelectedSource = 0;
		}
	}
	RebuildGrid();
	RebuildDetail();
}

void UCLArmoryWidget::SelectMake(FName MakeId)
{
	SelectedMake = MakeId;
	SelectedSource = 0;
	if (UCLLootRulesService* Rules = Loot())
	{
		const TArray<FCLWeaponSourceRef> Srcs = Rules->SourcesForMake(MakeId);
		const FCLWeaponSourceRef Primary = Rules->PrimarySourceForMake(MakeId);
		for (int32 i = 0; i < Srcs.Num(); ++i)
		{
			if (Srcs[i].TableId == Primary.TableId)
			{
				SelectedSource = i;
				break;
			}
		}
	}
	RebuildGrid();
	RebuildDetail();
}

void UCLArmoryWidget::SelectSource(int32 Index)
{
	SelectedSource = Index;
	RebuildSourceStrip();
	RebuildDetail();
}

void UCLArmoryWidget::RebuildGrid()
{
	if (!Grid)
	{
		return;
	}
	Grid->ClearChildren();
	UWidgetTree* Tree = HostTree.Get();
	UCLLootRulesService* Rules = Loot();
	if (!Tree || !Rules)
	{
		return;
	}
	const TArray<FCLWeaponMakeDef> Matches = Rules->MakesMatching(ClassFilter, MakerFilter);
	const int32 Cols = 3;
	for (int32 i = 0; i < Matches.Num(); ++i)
	{
		const FCLWeaponMakeDef& Make = Matches[i];
		UCLArmoryCellButton* Cell = Tree->ConstructWidget<UCLArmoryCellButton>(UCLArmoryCellButton::StaticClass(),
			*FString::Printf(TEXT("ArmoryCell_%s"), *Make.Id.ToString()));
		Cell->Configure(this, Make.Id, INDEX_NONE);
		UVerticalBox* Inner = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
			*FString::Printf(TEXT("ArmoryCellInner_%s"), *Make.Id.ToString()));
		UBorder* Thumb = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("ArmoryThumb_%s"), *Make.Id.ToString()));
		Thumb->SetBrushColor(ClassTint(Make.ClassId));
		USizeBox* ThumbSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("ArmoryThumbSize_%s"), *Make.Id.ToString()));
		ThumbSize->SetHeightOverride(48.f);
		ThumbSize->AddChild(Thumb);
		if (UVerticalBoxSlot* TSlot = Inner->AddChildToVerticalBox(ThumbSize))
		{
			TSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
		Inner->AddChildToVerticalBox(ArmoryLabel(Tree, *FString::Printf(TEXT("ArmoryCellName_%s"), *Make.Id.ToString()), Make.DisplayName, 12));
		Cell->AddChild(Inner);
		if (UButtonSlot* BSlot = Cast<UButtonSlot>(Inner->Slot))
		{
			BSlot->SetPadding(FMargin(6.f));
		}
		if (UUniformGridSlot* GSlot = Grid->AddChildToUniformGrid(Cell, i / Cols, i % Cols))
		{
			GSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UCLArmoryWidget::RebuildNestedList(UVerticalBox* Box, const TArray<FString>& Lines, const TCHAR* Empty)
{
	if (!Box)
	{
		return;
	}
	Box->ClearChildren();
	UWidgetTree* Tree = HostTree.Get();
	if (!Tree)
	{
		return;
	}
	if (Lines.Num() == 0)
	{
		Box->AddChildToVerticalBox(ArmoryLabel(Tree, *FString::Printf(TEXT("%sEmpty"), Empty), TEXT("None"), 12));
		return;
	}
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		Box->AddChildToVerticalBox(ArmoryLabel(Tree, *FString::Printf(TEXT("%s_%d"), Empty, i), Lines[i], 12));
	}
}

void UCLArmoryWidget::RebuildSourceStrip()
{
	if (!SourceRow)
	{
		return;
	}
	SourceRow->ClearChildren();
	UWidgetTree* Tree = HostTree.Get();
	UCLLootRulesService* Rules = Loot();
	if (!Tree || !Rules || SelectedMake.IsNone())
	{
		return;
	}
	const TArray<FCLWeaponSourceRef> Srcs = Rules->SourcesForMake(SelectedMake);
	for (int32 i = 0; i < Srcs.Num(); ++i)
	{
		UCLArmoryCellButton* Card = Tree->ConstructWidget<UCLArmoryCellButton>(UCLArmoryCellButton::StaticClass(),
			*FString::Printf(TEXT("ArmorySrc_%d"), i));
		Card->Configure(this, SelectedMake, i);
		const FString Label = FString::Printf(TEXT("%s\n%s"), KindLabel(Srcs[i].Source.Kind), *Srcs[i].Source.PathLabel());
		UTextBlock* Txt = ArmoryLabel(Tree, *FString::Printf(TEXT("ArmorySrcLbl_%d"), i), Label, 11);
		Card->AddChild(Txt);
		if (UHorizontalBoxSlot* Slot = SourceRow->AddChildToHorizontalBox(Card))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
	}
}

void UCLArmoryWidget::RebuildDetail()
{
	UCLLootRulesService* Rules = Loot();
	if (!Rules || SelectedMake.IsNone())
	{
		if (DetailName) DetailName->SetText(FText::FromString(TEXT("No makes in filter")));
		if (DetailMeta) DetailMeta->SetText(FText::GetEmpty());
		if (StatBlock) StatBlock->SetText(FText::GetEmpty());
		if (SourceRate) SourceRate->SetText(FText::GetEmpty());
		RebuildNestedList(ModsBox, TArray<FString>(), TEXT("Mod"));
		RebuildNestedList(PartsBox, TArray<FString>(), TEXT("Part"));
		return;
	}
	const FCLWeaponMakeDef* Make = Rules->FindWeaponMake(SelectedMake);
	if (!Make)
	{
		return;
	}
	const FCLWeaponMakerDef* Maker = Rules->FindWeaponMaker(Make->MakerId);
	const FCLWeaponCaliberDef* Cal = Rules->FindWeaponCaliber(Make->CaliberId);
	const FCLWeaponClassDef* Band = Rules->FindWeaponClass(Make->ClassId);
	if (DetailName)
	{
		DetailName->SetText(FText::FromString(Make->DisplayName));
	}
	FString Meta = Make->ClassId.ToString();
	if (Maker)
	{
		Meta += TEXT("  ·  ") + Maker->DisplayName;
	}
	if (Cal)
	{
		Meta += TEXT("  ·  ") + Cal->Id.ToString();
	}
	if (Make->bWorldOnly)
	{
		Meta += TEXT("  ·  world only");
	}
	if (DetailMeta)
	{
		DetailMeta->SetText(FText::FromString(Meta));
	}
	if (ConceptPlate && Band)
	{
		FLinearColor Tint = ClassTint(Make->ClassId);
		const TArray<FCLWeaponSourceRef> Srcs = Rules->SourcesForMake(SelectedMake);
		if (Srcs.IsValidIndex(SelectedSource) && !Srcs[SelectedSource].Variant.bBranded)
		{
			Tint *= 0.65f;
			Tint.A = 1.f;
		}
		ConceptPlate->SetBrushColor(Tint);
	}

	RebuildSourceStrip();
	const TArray<FCLWeaponSourceRef> Srcs = Rules->SourcesForMake(SelectedMake);
	FCLWeaponSourceRef Src = Rules->PrimarySourceForMake(SelectedMake);
	if (Srcs.IsValidIndex(SelectedSource))
	{
		Src = Srcs[SelectedSource];
	}
	FCLWeaponStats MinS;
	FCLWeaponStats MaxS;
	Rules->StatBandFor(SelectedMake, Src, MinS, MaxS);
	if (StatBlock)
	{
		StatBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Stability %.2f–%.2f\nHandling %.2f–%.2f\nRange %.2f–%.2f\nReload %.2f–%.2f\nGrip %.2f–%.2f\nMass %.2f–%.2f kg"),
			MinS.Stability, MaxS.Stability, MinS.Handling, MaxS.Handling, MinS.Range, MaxS.Range,
			MinS.Reload, MaxS.Reload, MinS.Grip, MaxS.Grip, MinS.MassKg, MaxS.MassKg)));
	}
	if (SourceRate)
	{
		const float Rate = Src.AdvertisedRate();
		const int32 OneIn = Rate > 0.f ? FMath::Max(1, FMath::RoundToInt(1.f / Rate)) : 0;
		SourceRate->SetText(FText::FromString(FString::Printf(
			TEXT("%s  ·  advertised ~1 in %d  ·  %s"),
			*Src.Source.PathLabel(),
			OneIn,
			Src.Variant.bBranded ? TEXT("branded / full band") : TEXT("plain / narrower band"))));
	}

	TArray<FString> ModLines;
	for (const FCLModifierDef& M : Rules->ModsForMake(SelectedMake))
	{
		ModLines.Add(FString::Printf(TEXT("%s  (%s)"), *M.DisplayName, *M.Type));
	}
	TArray<FString> PartLines;
	for (const FCLWeaponPartDef& P : Rules->PartsForMake(SelectedMake))
	{
		PartLines.Add(P.DisplayName);
	}
	RebuildNestedList(ModsBox, ModLines, TEXT("Mod"));
	RebuildNestedList(PartsBox, PartLines, TEXT("Part"));
}
