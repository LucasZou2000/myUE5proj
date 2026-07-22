#include "Loot/LootContainerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryGridLibrary.h"
#include "Items/ItemDefinition.h"
#include "Loot/LootContainer.h"
#include "MYPROJ2PlayerController.h"

namespace
{
	constexpr float CellSize = 42.0f;

	UTextBlock* MakeLootText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		Label->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = Size;
		Label->SetFont(Font);
		return Label;
	}
}

void ULootItemButton::Configure(ULootContainerWidget* InOwner, const FGuid& InItemId, bool bInContainerItem)
{
	OwnerWidget = InOwner;
	ItemId = InItemId;
	bContainerItem = bInContainerItem;
	OnClicked.AddUniqueDynamic(this, &ULootItemButton::HandleClicked);
}

void ULootItemButton::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->HandleItemClicked(ItemId, bContainerItem);
	}
}

void ULootContainerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	RebuildGrids();

	if (OwningRaidController)
	{
		OwningRaidController->SetInputMode(FInputModeGameAndUI());
		OwningRaidController->bShowMouseCursor = true;
		if (UInventoryComponent* Inventory = OwningRaidController->GetInventoryComponent())
		{
			Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ULootContainerWidget::HandleInventoryChanged);
		}
	}
	if (LootContainer && LootContainer->GetInventoryComponent())
	{
		LootContainer->GetInventoryComponent()->OnInventoryChanged.AddUniqueDynamic(this, &ULootContainerWidget::HandleInventoryChanged);
	}
}

void ULootContainerWidget::NativeDestruct()
{
	if (OwningRaidController && OwningRaidController->GetInventoryComponent())
	{
		OwningRaidController->GetInventoryComponent()->OnInventoryChanged.RemoveDynamic(this, &ULootContainerWidget::HandleInventoryChanged);
	}
	if (LootContainer && LootContainer->GetInventoryComponent())
	{
		LootContainer->GetInventoryComponent()->OnInventoryChanged.RemoveDynamic(this, &ULootContainerWidget::HandleInventoryChanged);
	}
	if (OwningRaidController)
	{
		OwningRaidController->SetInputMode(FInputModeGameOnly());
	}
	Super::NativeDestruct();
}

void ULootContainerWidget::InitializeForContainer(AMYPROJ2PlayerController* InController, ALootContainer* InContainer)
{
	OwningRaidController = InController;
	LootContainer = InContainer;
	if (IsConstructed())
	{
		RebuildGrids();
	}
}

void ULootContainerWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Shade = WidgetTree->ConstructWidget<UBorder>();
	Shade->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.02f, 0.72f));
	UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade);
	ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
	ShadeSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* Window = WidgetTree->ConstructWidget<UBorder>();
	Window->SetBrushColor(FLinearColor(0.055f, 0.065f, 0.07f, 0.98f));
	Window->SetPadding(FMargin(22.0f));
	UOverlaySlot* WindowSlot = Root->AddChildToOverlay(Window);
	WindowSlot->SetHorizontalAlignment(HAlign_Center);
	WindowSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Window->SetContent(Layout);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Header);
	UTextBlock* Title = MakeLootText(WidgetTree, FText::FromString(TEXT("LOOT CONTAINER")), 20, FLinearColor(0.92f, 0.94f, 0.90f));
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>();
	CloseButton->OnClicked.AddDynamic(this, &ULootContainerWidget::HandleCloseClicked);
	CloseButton->SetContent(MakeLootText(WidgetTree, FText::FromString(TEXT("X")), 16, FLinearColor::White));
	Header->AddChildToHorizontalBox(CloseButton);

	USpacer* HeaderGap = WidgetTree->ConstructWidget<USpacer>();
	HeaderGap->SetSize(FVector2D(1.0f, 16.0f));
	Layout->AddChildToVerticalBox(HeaderGap);
	UHorizontalBox* Grids = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Grids);

	auto AddGridPanel = [this, Grids](const FString& Name, const FIntPoint& Size, TObjectPtr<UGridPanel>& OutGrid)
	{
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Grids->AddChildToHorizontalBox(Column);
		Column->AddChildToVerticalBox(MakeLootText(WidgetTree, FText::FromString(Name), 14, FLinearColor(0.65f, 0.72f, 0.68f)));
		USpacer* Gap = WidgetTree->ConstructWidget<USpacer>();
		Gap->SetSize(FVector2D(1.0f, 8.0f));
		Column->AddChildToVerticalBox(Gap);
		USizeBox* GridSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		GridSizeBox->SetWidthOverride(Size.X * CellSize);
		GridSizeBox->SetHeightOverride(Size.Y * CellSize);
		Column->AddChildToVerticalBox(GridSizeBox);
		OutGrid = WidgetTree->ConstructWidget<UGridPanel>();
		GridSizeBox->SetContent(OutGrid);
	};

	AddGridPanel(TEXT("RAID INVENTORY"), FIntPoint(10, 6), PlayerGrid);
	USpacer* MiddleGap = WidgetTree->ConstructWidget<USpacer>();
	MiddleGap->SetSize(FVector2D(28.0f, 1.0f));
	Grids->AddChildToHorizontalBox(MiddleGap);
	AddGridPanel(TEXT("CRATE"), FIntPoint(5, 8), ContainerGrid);
}

void ULootContainerWidget::BuildInventoryGrid(UGridPanel* Grid, UInventoryComponent* Inventory, bool bContainerInventory)
{
	if (!Grid || !Inventory)
	{
		return;
	}
	Grid->ClearChildren();
	const FIntPoint GridSize = Inventory->GetGridSize();
	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			UBorder* Cell = WidgetTree->ConstructWidget<UBorder>();
			Cell->SetBrushColor(((X + Y) % 2 == 0) ? FLinearColor(0.10f, 0.115f, 0.12f, 1.0f) : FLinearColor(0.085f, 0.095f, 0.10f, 1.0f));
			Cell->SetPadding(FMargin(1.0f));
			Grid->AddChildToGrid(Cell, Y, X);
		}
	}

	for (const FReplicatedInventoryEntry& Entry : Inventory->GetEntries())
	{
		UItemDefinition* Definition = Inventory->ResolveDefinition(Entry.Item.DefinitionId);
		if (!Definition)
		{
			continue;
		}
		const FIntPoint Footprint = Definition->GetEffectiveGridSize(Entry.Item.bRotated);
		ULootItemButton* Button = WidgetTree->ConstructWidget<ULootItemButton>();
		Button->Configure(this, Entry.Item.InstanceId, bContainerInventory);
		FButtonStyle Style = Button->GetStyle();
		Style.Normal.TintColor = FSlateColor(bContainerInventory ? FLinearColor(0.20f, 0.32f, 0.27f, 1.0f) : FLinearColor(0.22f, 0.28f, 0.34f, 1.0f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(0.34f, 0.48f, 0.40f, 1.0f));
		Button->SetStyle(Style);
		const FString Name = Definition->DisplayName.IsEmpty() ? Definition->ItemId.ToString() : Definition->DisplayName.ToString();
		Button->SetContent(MakeLootText(WidgetTree, FText::FromString(FString::Printf(TEXT("%s\nx%d"), *Name, Entry.Item.Quantity)), 11, FLinearColor::White));
		UGridSlot* ItemGridSlot = Grid->AddChildToGrid(Button, Entry.Item.GridPosition.Y, Entry.Item.GridPosition.X);
		ItemGridSlot->SetColumnSpan(Footprint.X);
		ItemGridSlot->SetRowSpan(Footprint.Y);
	}
}

void ULootContainerWidget::RebuildGrids()
{
	if (!OwningRaidController || !LootContainer)
	{
		return;
	}
	BuildInventoryGrid(PlayerGrid, OwningRaidController->GetInventoryComponent(), false);
	BuildInventoryGrid(ContainerGrid, LootContainer->GetInventoryComponent(), true);
}

void ULootContainerWidget::HandleInventoryChanged()
{
	RebuildGrids();
}

void ULootContainerWidget::HandleCloseClicked()
{
	RemoveFromParent();
}

void ULootContainerWidget::HandleItemClicked(const FGuid& ItemId, bool bFromContainer)
{
	if (!OwningRaidController || !LootContainer)
	{
		return;
	}
	UInventoryComponent* PlayerInventory = OwningRaidController->GetInventoryComponent();
	UInventoryComponent* LootInventory = LootContainer->GetInventoryComponent();
	UInventoryComponent* Source = bFromContainer ? LootInventory : PlayerInventory;
	const FReplicatedInventoryEntry* Entry = Source ? Source->FindEntry(ItemId) : nullptr;
	if (!Entry)
	{
		return;
	}
	const uint16 RequestId = PlayerInventory->AllocateRequestId();
	if (bFromContainer)
	{
		OwningRaidController->ServerTakeFromContainer(LootContainer, ItemId, Entry->Item.Quantity, RequestId);
		return;
	}

	UItemDefinition* Definition = PlayerInventory->ResolveDefinition(Entry->Item.DefinitionId);
	if (!Definition)
	{
		return;
	}
	TFunction<const FItemInstance&(const FReplicatedInventoryEntry&)> GetItem =
		[](const FReplicatedInventoryEntry& Value) -> const FItemInstance& { return Value.Item; };
	TFunction<FIntPoint(const FItemInstance&)> GetFootprint = [LootInventory](const FItemInstance& Value)
	{
		if (UItemDefinition* ItemDefinition = LootInventory->ResolveDefinition(Value.DefinitionId))
		{
			return ItemDefinition->GetEffectiveGridSize(Value.bRotated);
		}
		return FIntPoint(1, 1);
	};
	FIntPoint TargetPosition = FIntPoint::ZeroValue;
	FInventoryGridLibrary::FindFirstFit(LootInventory->GetGridSize(), LootInventory->GetEntries(), GetItem,
		GetFootprint, Definition->GetEffectiveGridSize(Entry->Item.bRotated), TargetPosition);
	OwningRaidController->ServerPutIntoContainer(LootContainer, ItemId, Entry->Item.Quantity,
		TargetPosition, Entry->Item.bRotated, RequestId);
}
