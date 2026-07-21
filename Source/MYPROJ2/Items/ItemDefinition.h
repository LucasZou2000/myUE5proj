#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ItemDefinition.generated.h"

/**
 * Static item classification. M3 ships Material/Consumable; later milestones
 * use Weapon/WeaponPart/Quest.
 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	Material,
	Consumable,
	Weapon,
	WeaponPart,
	Quest
};

/**
 * Static definition of an item type. Lives only in /Game/Raid/Data/Items.
 *
 * The Primary Asset ID (`Item:<ItemId>`) is the stable runtime/save/network
 * identifier. The asset filename/path may change without breaking saves.
 *
 * Per m3-inventory-items.md, runtime entries store IDs, never copies of
 * display data or hard UObject pointers.
 */
UCLASS(BlueprintType)
class MYPROJ2_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Stable, unique item name (e.g. "Scrap", "Medkit"). Must be non-empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemType ItemType = EItemType::Material;

	/** Grid footprint (width, height) before rotation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(1, 1);

	/** Maximum stack quantity per grid cell. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStack = 1;

	/**
	 * Baseline raid value for loot budgeting. This is deliberately static item
	 * data: container generation applies map/container multipliers separately.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Loot", meta = (ClampMin = "0"))
	int32 LootValue = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer ItemTags;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Item"), ItemId);
	}

	/** Footprint after applying the rotated flag. */
	FIntPoint GetEffectiveGridSize(bool bRotated) const
	{
		return bRotated ? FIntPoint(GridSize.Y, GridSize.X) : GridSize;
	}
};

/**
 * M3 Medkit. Server resolves this definition for UseItem and calls the
 * authority heal on the owning character's UHealthComponent.
 */
UCLASS(BlueprintType)
class MYPROJ2_API UConsumableItemDefinition : public UItemDefinition
{
	GENERATED_BODY()

public:

	UConsumableItemDefinition()
	{
		ItemType = EItemType::Consumable;
	}

	/** Healing applied on use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0"))
	float HealAmount = 0.0f;
};
