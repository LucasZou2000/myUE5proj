#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "LootContainerDefinition.generated.h"

class UDataTable;

/** Tuning shared by a class of world loot containers. */
UCLASS(BlueprintType)
class MYPROJ2_API ULootContainerDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FName LootPoolId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ClampMax = "16"))
	int32 Rolls = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDataTable> LootTable;

	/** Width x height. M4 default is 5 x 8; future containers may use heights up to 10. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ClampMax = "10"))
	FIntPoint GridSize = FIntPoint(5, 8);

	/** Expected value before map multiplier and random variation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Value", meta = (ClampMin = "0"))
	int32 BaseTargetValue = 100;

	/** Symmetric fractional budget variation. 0.15 means [85%, 115%]. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Value", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetValueVariance = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FGameplayTagContainer ZoneTags;
};
