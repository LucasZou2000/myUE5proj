#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/InventoryTypes.h"
#include "InventoryComponent.generated.h"

class UItemDefinition;
class UConsumableItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedSignature);

/**
 * Server-authoritative grid inventory with Fast Array replication.
 *
 * M3 placement: on `AMYPROJ2PlayerController`. A PlayerController exists only
 * on the Server and its owning client, so the replicated entries are
 * naturally private without per-connection filters. Pawn replacement does not
 * destroy carried inventory.
 *
 * M4 will place the same component on world containers; replication then
 * follows the container Actor's relevancy.
 *
 * All mutations are authority-only and call `MarkItemDirty` / `MarkArrayDirty`.
 * Client RPCs express intent (`ServerMoveItem`, `ServerSplitStack`, ...);
 * the Server revalidates everything.
 */
UCLASS(ClassGroup=(MYPROJ2), meta=(BlueprintSpawnableComponent))
class MYPROJ2_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	//~ Begin Configuration

	/** Grid columns (positive X). 10 per gameplay-contracts.json. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 GridColumns = 10;

	/** Grid rows (positive Y). 6 per gameplay-contracts.json. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 GridRows = 6;

	/** Editor-only seed items used to test inventory in PIE. Server-only. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Debug")
	bool bSeedTestItems = false;

	/** Test items granted on Server when bSeedTestItems is true. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Debug", meta = (EditCondition = "bSeedTestItems"))
	TArray<TObjectPtr<UItemDefinition>> SeedItems;

	//~ End Configuration

	//~ Begin State (replicated)

	/** Fast Array of inventory entries. Replicated to the owning client only. */
	UPROPERTY(Replicated)
	FReplicatedInventoryList ReplicatedItems;

	//~ End State

	/** Fired on both Server and owning client whenever entries change. UI binds here. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryChangedSignature OnInventoryChanged;

	//~ Begin Public Queries

	/** Read-only view of entries (local view on Server or owning client). */
	const TArray<FReplicatedInventoryEntry>& GetEntries() const { return ReplicatedItems.Entries; }

	/** Look up an entry by instance id. Returns nullptr when absent. */
	const FReplicatedInventoryEntry* FindEntry(const FGuid& InstanceId) const;
	FReplicatedInventoryEntry* FindEntryMutable(const FGuid& InstanceId);

	/** Resolve the static definition for an entry via AssetManager. Server + owning client only. */
	UItemDefinition* ResolveDefinition(const FPrimaryAssetId& DefinitionId) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FIntPoint GetGridSize() const { return FIntPoint(GridColumns, GridRows); }

	/**
	 * M3 debug helper: returns a formatted multi-line string describing the
	 * current inventory contents. Suitable for a single TextBlock.
	 * Example output:
	 *   [0] Scrap x20 (1x1) at (0,0)
	 *   [1] Medkit x3 (2x1) at (1,0)
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
	FString GetInventoryDisplayText() const;

	/**
	 * M3 debug helper: returns one formatted string per entry, suitable for
	 * populating a VerticalBox or ListView in UMG.
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
	TArray<FString> GetInventoryEntryStrings() const;

	/**
	 * M3 debug helper: server-only. Grants the test seed items defined in
	 * `SeedItems`. Useful for PIE testing when bSeedTestItems was not set
	 * before BeginPlay.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Inventory|Debug")
	void DebugGrantSeedItems();

	//~ End Public Queries

	//~ Begin Authority Mutators (Server only; not RPCs)

	/**
	 * Authority-only. Adds `Quantity` of `Definition`, merging into existing
	 * compatible stacks first and then placing first-fit. Returns true when
	 * the full quantity was added.
	 */
	bool AuthorityTryAdd(UItemDefinition* Definition, int32 Quantity);

	/**
	 * Authority-only. Removes up to `Quantity` from the entry matching
	 * `InstanceId`. Returns true when the full quantity was removed.
	 */
	bool AuthorityTryRemove(const FGuid& InstanceId, int32 Quantity);

	/**
	 * Atomically transfers a full quantity to another authoritative inventory.
	 * The destination is validated from a staged copy before either Fast Array
	 * changes. An unset TargetPosition chooses deterministic first-fit.
	 */
	bool AuthorityTransferTo(UInventoryComponent* Destination, const FGuid& InstanceId,
		int32 Quantity, const TOptional<FIntPoint>& TargetPosition,
		bool bRotated, EInventoryRejectReason& OutReason);

	//~ End Authority Mutators

	//~ Begin RPC contract (m3-inventory-items.md)

	UFUNCTION(Server, Reliable)
	void ServerMoveItem(FGuid ItemId, FIntPoint Position, bool bRotated, uint16 RequestId);

	UFUNCTION(Server, Reliable)
	void ServerSplitStack(FGuid ItemId, int32 Quantity, FIntPoint Position, bool bRotated, uint16 RequestId);

	UFUNCTION(Server, Reliable)
	void ServerMergeStacks(FGuid SourceId, FGuid TargetId, int32 Quantity, uint16 RequestId);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(FGuid ItemId, uint16 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientInventoryRejected(uint16 RequestId, EInventoryRejectReason Reason);

	//~ End RPC contract

	/** Client-side helper to allocate the next monotonic request id. */
	uint16 AllocateRequestId() { return ++NextRequestId; }

	/** Last request id we successfully sent from this client. Read by UI for rejection mapping. */
	uint16 GetLastIssuedRequestId() const { return NextRequestId; }

protected:

	/** Server-side request-id dedupe. Stale requests are rejected without mutation. */
	bool IsRequestStale(uint16 RequestId) const;
	void MarkRequestConsumed(uint16 RequestId);

	/** Authority-only implementations. Each returns false + sets OutReason on failure. */
	bool AuthorityMoveItem(const FGuid& ItemId, FIntPoint Position, bool bRotated, EInventoryRejectReason& OutReason);
	bool AuthoritySplitStack(const FGuid& ItemId, int32 Quantity, FIntPoint Position, bool bRotated, EInventoryRejectReason& OutReason);
	bool AuthorityMergeStacks(const FGuid& SourceId, const FGuid& TargetId, int32 Quantity, EInventoryRejectReason& OutReason);
	bool AuthorityUseItem(const FGuid& ItemId, EInventoryRejectReason& OutReason);

	/** Compute the effective footprint for an entry using its resolved definition. */
	FIntPoint GetEntryFootprint(const FItemInstance& Entry) const;

	/** Push an entry into the Fast Array (Server only). Caller must have validated placement. */
	void CommitNewEntry(FItemInstance&& NewItem);

	/** Mark an entry dirty after in-place mutation (Server only). */
	void MarkEntryDirty(FReplicatedInventoryEntry& Entry);

	/** Remove an entry from the Fast Array (Server only). */
	void RemoveEntry(const FGuid& InstanceId);

	/** Fast Array replication callbacks. */
	void OnRep_EntriesChanged();

private:

	/** Monotonic request counter on the owning client. */
	uint16 NextRequestId = 0;

	/** Server-side last consumed request id. Used for stale-replay rejection. */
	uint16 LastConsumedRequestId = 0;

	/** True when LastConsumedRequestId has been initialised (first request always passes). */
	bool bHasConsumedRequest = false;

	friend struct FReplicatedInventoryList;
	friend struct FReplicatedInventoryEntry;
};
