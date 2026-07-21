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

	/** Legacy authored field. M4 generation now stops at the value lower bound instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ClampMax = "16", DeprecatedProperty, DeprecationMessage = "Generation uses BaseTargetValue instead."))
	int32 Rolls = 4;

	/** Hard safety cap for incremental placement attempts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ClampMax = "256"))
	int32 MaxGenerationAttempts = 64;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDataTable> LootTable;

	/** Width x height. M4 default is 5 x 8; future containers may use heights up to 10. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ClampMax = "10"))
	FIntPoint GridSize = FIntPoint(5, 8);

	/** Value lower bound before the map multiplier. Generation stops after reaching or exceeding it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Value", meta = (ClampMin = "0"))
	int32 BaseTargetValue = 100;

	/** Legacy authored field retained for existing assets; value lower bounds are no longer randomized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Value", meta = (ClampMin = "0.0", ClampMax = "1.0", DeprecatedProperty, DeprecationMessage = "Generation uses an exact value lower bound."))
	float TargetValueVariance = 0.15f;

	/** Cells deliberately left vacant even when the value lower bound has not yet been reached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot|Placement", meta = (ClampMin = "0"))
	int32 ReservedEmptyCells = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	FGameplayTagContainer ZoneTags;
};
