#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"

class ULootContainerDefinition;
class UItemDefinition;

/** Pure-ish server helper: its only external input is item-definition resolution. */
struct MYPROJ2_API FLootGenerationLibrary
{
	static int32 MakeSeed(int32 RaidSeed, const FGuid& ContainerId, int32 GenerationVersion);

	static bool Generate(const ULootContainerDefinition& Definition, int32 RaidSeed,
		const FGuid& ContainerId, int32 GenerationVersion, float MapValueMultiplier,
		TArray<FItemInstance>& OutItems);

	static UItemDefinition* ResolveItemDefinition(const FPrimaryAssetId& DefinitionId);
};
