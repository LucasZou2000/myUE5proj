#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "LootContainerWidget.generated.h"

class ALootContainer;
class AMYPROJ2PlayerController;
class UGridPanel;
class UInventoryComponent;
class ULootContainerWidget;

/** Button carrying the stable item identity for a grid entry. */
UCLASS()
class MYPROJ2_API ULootItemButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(ULootContainerWidget* InOwner, const FGuid& InItemId, bool bInContainerItem);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	TObjectPtr<ULootContainerWidget> OwnerWidget;

	FGuid ItemId;
	bool bContainerItem = false;
};

/** Minimal functional M4 two-grid loot UI, built natively to avoid Blueprint gameplay logic. */
UCLASS()
class MYPROJ2_API ULootContainerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForContainer(AMYPROJ2PlayerController* InController, ALootContainer* InContainer);
	void HandleItemClicked(const FGuid& ItemId, bool bFromContainer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RebuildGrids();
	void BuildInventoryGrid(UGridPanel* Grid, UInventoryComponent* Inventory, bool bContainerInventory);

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	TObjectPtr<AMYPROJ2PlayerController> OwningRaidController;

	UPROPERTY()
	TObjectPtr<ALootContainer> LootContainer;

	UPROPERTY()
	TObjectPtr<UGridPanel> PlayerGrid;

	UPROPERTY()
	TObjectPtr<UGridPanel> ContainerGrid;
};
