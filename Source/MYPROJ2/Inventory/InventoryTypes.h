#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "InventoryTypes.generated.h"

class UInventoryComponent;

/**
 * A single runtime item entry. Stores only IDs and grid state; display data
 * is resolved through the AssetManager when needed.
 */
USTRUCT(BlueprintType)
struct MYPROJ2_API FItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 Quantity = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FIntPoint GridPosition = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadWrite, Category = "Item")
	bool bRotated = false;

	bool IsValid() const
	{
		return InstanceId.IsValid() && DefinitionId.IsValid() && Quantity > 0;
	}

	bool operator==(const FItemInstance& Other) const
	{
		return InstanceId == Other.InstanceId;
	}

	bool operator!=(const FItemInstance& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Fast Array entry wrapping an FItemInstance. M3 replication uses delta
 * serialization so a single moved item sends only that entry, not the full list.
 */
USTRUCT()
struct MYPROJ2_API FReplicatedInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FItemInstance Item;

	/** Server-side cached pointer so post-replication hooks can resolve the owner. */
	TObjectPtr<UInventoryComponent> OwnerComponent = nullptr;

	void PostReplicatedAdd(const FReplicatedInventoryList& InArraySerializer);
	void PostReplicatedChange(const FReplicatedInventoryList& InArraySerializer);
	void PreReplicatedRemove(const FReplicatedInventoryList& InArraySerializer);
};

/**
 * Fast Array serializer for the player's raid inventory.
 *
 * Owned by UInventoryComponent on the PlayerController. A PlayerController
 * exists only on the Server and its owning client, so the replicated entries
 * are naturally private without per-connection filters.
 */
USTRUCT()
struct MYPROJ2_API FReplicatedInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FReplicatedInventoryEntry> Entries;

	/** Not replicated; set on the component so post-replication hooks can route. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UInventoryComponent> OwnerComponent = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedInventoryEntry, FReplicatedInventoryList>(
			Entries, DeltaParams, *this);
	}

	void SetOwner(UInventoryComponent* InOwner)
	{
		OwnerComponent = InOwner;
		for (FReplicatedInventoryEntry& Entry : Entries)
		{
			Entry.OwnerComponent = InOwner;
		}
	}
};

template<>
struct TStructOpsTypeTraits<FReplicatedInventoryList> : public TStructOpsTypeTraitsBase2<FReplicatedInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/** Rejection codes sent back to the owning client. */
UENUM(BlueprintType)
enum class EInventoryRejectReason : uint8
{
	None,
	InvalidRequest,
	StaleRequest,
	MissingItem,
	InvalidDefinition,
	OutOfBounds,
	Overlap,
	StackMismatch,
	StackFull,
	NoSpace,
	NotUsable,
	Dead,
	FullHealth
};
