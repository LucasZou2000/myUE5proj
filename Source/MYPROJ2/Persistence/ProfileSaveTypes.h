#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"
#include "ProfileSaveTypes.generated.h"

/** Plain, local-only grid inventory persisted by the player profile. */
USTRUCT(BlueprintType)
struct MYPROJ2_API FSavedInventory
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Profile")
	FIntPoint GridSize = FIntPoint(5, 20);

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Profile")
	TArray<FItemInstance> Items;
};

/** Items and cash staged in the base, ready to be consumed by the next local raid. */
USTRUCT(BlueprintType)
struct MYPROJ2_API FPreparedRaidLoadout
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Profile")
	FSavedInventory Inventory;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Profile")
	int64 Currency = 0;
};

/** Server-confirmed result delivered to the owning local profile after a raid ends. */
USTRUCT(BlueprintType)
struct MYPROJ2_API FRaidSettlementPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	bool bExtracted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	TArray<FItemInstance> Items;

	UPROPERTY(BlueprintReadOnly, Category = "Raid")
	int64 CarriedCurrency = 0;
};
