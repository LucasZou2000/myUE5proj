# M3 Inventory and Items

Depends on: M2 accepted, `network-architecture.md`, and `gameplay-contracts.json`.

## Objective

Implement one server-authoritative grid inventory for each player, static item definitions, stacking, move/rotate/split/merge/use operations, and a minimal owner-only inventory UI. M3 does not add world loot containers; that is M4.

## Architecture decision

The raid inventory component lives on `AMYPROJ2PlayerController`, not the Character. A PlayerController exists only on the Server and its owning client, so its replicated Fast Array is naturally private without per-connection replication filters. Character replacement does not destroy carried inventory.

`UInventoryComponent` is reusable. In M4 the same component is placed on a replicated world container; its data then follows that Actor's relevancy and is visible to relevant clients. Per-viewer container privacy is intentionally out of scope.

## M2 hardening prerequisite

Before inventory work, add named collision channels `WeaponTrace` and `InteractionTrace` in `DefaultEngine.ini`. Move combat off `ECC_Visibility` to `WeaponTrace`; move interaction focus/LOS to `InteractionTrace`. Verify the M2 cube can still be shot and the M1 interactable can still be used. Shared `Visibility` is not acceptable once containers and UI-targetable world actors exist.

## Required files

```text
Source/MYPROJ2/Items/ItemDefinition.h/.cpp
Source/MYPROJ2/Inventory/InventoryTypes.h
Source/MYPROJ2/Inventory/InventoryGridLibrary.h/.cpp
Source/MYPROJ2/Inventory/InventoryComponent.h/.cpp
Source/MYPROJ2/UI/RaidInventoryViewModel.h/.cpp (optional if direct widget binding stays small)
```

Modify `AMYPROJ2PlayerController` to own `UInventoryComponent`. Add `GameplayTags` to the module dependency list. Do not put UI widgets or texture references in the runtime item instance struct.

## Static definitions

```cpp
UENUM(BlueprintType)
enum class EItemType : uint8 { Material, Consumable, Weapon, WeaponPart, Quest };

UCLASS(Abstract, BlueprintType)
class UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) FName ItemId;
    UPROPERTY(EditDefaultsOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditDefaultsOnly) EItemType ItemType;
    UPROPERTY(EditDefaultsOnly) FIntPoint GridSize = FIntPoint(1, 1);
    UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1")) int32 MaxStack = 1;
    UPROPERTY(EditDefaultsOnly) FGameplayTagContainer ItemTags;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    { return FPrimaryAssetId(TEXT("Item"), ItemId); }
};

UCLASS(BlueprintType)
class UConsumableItemDefinition : public UItemDefinition
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0")) float HealAmount = 0.0f;
};
```

The explicit `ItemId` must be non-empty and unique. The Primary Asset ID (`Item:<ItemId>`) is the stable item ID; asset filename/path may change. Configure AssetManager type `Item` scanning for `/Game/Raid/Data/Items`. Runtime/save/network state stores IDs, never copied display data or hard UObject pointers.

## Runtime and replication types

```cpp
USTRUCT(BlueprintType)
struct FItemInstance
{
    GENERATED_BODY()
    UPROPERTY() FGuid InstanceId;
    UPROPERTY() FPrimaryAssetId DefinitionId;
    UPROPERTY() int32 Quantity = 1;
    UPROPERTY() FIntPoint GridPosition = FIntPoint::ZeroValue;
    UPROPERTY() bool bRotated = false;
};

USTRUCT()
struct FReplicatedInventoryEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()
    UPROPERTY() FItemInstance Item;
};

USTRUCT()
struct FReplicatedInventoryList : public FFastArraySerializer
{
    GENERATED_BODY()
    UPROPERTY() TArray<FReplicatedInventoryEntry> Entries;
    UPROPERTY(NotReplicated) TObjectPtr<UInventoryComponent> OwnerComponent;
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
};
```

Add the required `TStructOpsTypeTraits` specialization. Every mutation occurs on Authority and calls `MarkItemDirty` or `MarkArrayDirty`. `PostReplicatedAdd/Change/Remove` emits presentation delegates only.

## Grid rules

- Coordinates are zero-based, top-left, with positive X right and positive Y down.
- Rotating swaps definition width/height.
- Bounds are exclusive at `GridSize.X/Y`.
- Collision tests exclude the entry being moved.
- Stack merge requires identical definition ID and available `MaxStack` space.
- First-fit is deterministic row-major order. Merge compatible stacks before consuming a new cell.
- An operation either commits completely or leaves the inventory unchanged.
- Core placement/merge algorithms are pure functions in `InventoryGridLibrary` and receive definitions through resolved dimensions/max stack, not global lookups.

## Component contract

```cpp
UENUM(BlueprintType)
enum class EInventoryRejectReason : uint8
{
    None, InvalidRequest, StaleRequest, MissingItem, InvalidDefinition,
    OutOfBounds, Overlap, StackMismatch, StackFull, NoSpace, NotUsable, Dead
};

UFUNCTION(Server, Reliable)
void ServerMoveItem(FGuid ItemId, FIntPoint Position, bool bRotated, uint16 RequestId);

UFUNCTION(Server, Reliable)
void ServerSplitStack(FGuid ItemId, int32 Quantity, FIntPoint Position,
                      bool bRotated, uint16 RequestId);

UFUNCTION(Server, Reliable)
void ServerMergeStacks(FGuid SourceId, FGuid TargetId, int32 Quantity,
                       uint16 RequestId);

UFUNCTION(Server, Reliable)
void ServerUseItem(FGuid ItemId, uint16 RequestId);

UFUNCTION(Client, Reliable)
void ClientInventoryRejected(uint16 RequestId, EInventoryRejectReason Reason);
```

Public mutators such as `AuthorityTryAdd`, `AuthorityTryRemove`, and `AuthorityTransferTo` are non-RPC authority-only C++ methods. `AuthorityTransferTo` is added in M4 but its transaction shape should be reserved now. Request IDs are monotonic and compared wrap-safely.

The UI is not optimistic in M3: it displays a local drag ghost, sends a request, and commits the slot only after Fast Array replication. Rejection restores the displayed source slot and shows no modal dialog.

## Item use

M3 ships one Medkit. Server resolves `UConsumableItemDefinition`, rejects dead/full-health use, calls the existing `UHealthComponent` authority heal method, decrements one item, and removes an empty stack. Add healing as an authority-only health operation; do not expose a client health setter.

## MCP assets

- `/Game/Raid/Data/Items/DA_Item_Scrap`
- `/Game/Raid/Data/Items/DA_Item_Medkit`
- `/Game/Raid/UI/WBP_InventoryGrid`
- `IA_Inventory` and its mapping in `IMC_Raid`

Use simple generated/engine icons if needed. Widget layout must support 10x6 cells, drag ghost, quantity label, rotated footprint, and rejection refresh. MCP-created assets must be compiled, saved, and inspected in PIE.

## Implementation order

1. Split collision channels and rerun M1/M2 smoke tests.
2. Add item definitions and AssetManager scan configuration.
3. Implement/test pure grid functions.
4. Implement Fast Array component and authority mutation API.
5. Attach component to PlayerController and seed Server test items behind an editor-only setting.
6. Add RPC requests/rejections and Medkit use.
7. Add minimal inventory UI and two-player PIE validation.

## Acceptance criteria

- Owner sees inventory; another client cannot inspect it through replicated PlayerController state.
- Add/move/rotate/split/merge/use converge under 100ms RTT and 2% loss.
- Invalid overlap, bounds, quantities, IDs, replayed request IDs, and full-health Medkit use do not mutate state.
- Two simultaneous commands are serialized and cannot duplicate or lose items.
- Reconnecting/host migration is not required; Pawn destruction and respawn preserve Controller inventory.
- Fast Array sends deltas rather than rebuilding the full list for one moved item.
- M1 interaction and M2 firing still pass on their dedicated collision channels.

## Stop conditions

Do not start M4 if any mutation is performed directly by a widget/client, item definitions are copied into runtime entries, grid logic depends on UMG, or a rejected transaction partially changes quantity/position.
