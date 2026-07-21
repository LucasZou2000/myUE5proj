#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "LootTypes.generated.h"

/** One weighted candidate in a container loot pool. */
USTRUCT(BlueprintType)
struct MYPROJ2_API FLootTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName LootPoolId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	/** Uses this row only to close a value gap after ordinary placements cannot fit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Value")
	bool bCanFillValueGap = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FGameplayTagContainer RequiredZoneTags;
};
