# M4 Loot and Containers

Depends on: M3 accepted and `gameplay-contracts.json`.

## Objective

Add server-generated loot containers, deterministic weighted loot tables, replicated container inventory, and atomic player/container transfers that are safe when multiple players loot simultaneously.

## Required files

```text
Source/MYPROJ2/Loot/LootTypes.h
Source/MYPROJ2/Loot/LootContainerDefinition.h/.cpp
Source/MYPROJ2/Loot/LootGenerationLibrary.h/.cpp
Source/MYPROJ2/Loot/LootContainer.h/.cpp
```

Modify PlayerController with the client open/close notification and owned transfer RPC entry points. Do not put client-callable Server RPCs on `ALootContainer`; clients do not own it.

## Loot data

```cpp
USTRUCT(BlueprintType)
struct FLootTableRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName LootPoolId;
    UPROPERTY(EditAnywhere) FPrimaryAssetId ItemDefinitionId;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.0")) float Weight = 1.0f;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 MinQuantity = 1;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 MaxQuantity = 1;
    UPROPERTY(EditAnywhere) FGameplayTagContainer RequiredZoneTags;
};

UCLASS(BlueprintType)
class ULootContainerDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) FName LootPoolId;
    UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1", ClampMax="16")) int32 Rolls = 4;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UDataTable> LootTable;
    UPROPERTY(EditDefaultsOnly) FIntPoint GridSize = FIntPoint(5, 8);
    UPROPERTY(EditDefaultsOnly) int32 BaseTargetValue = 100;
    UPROPERTY(EditDefaultsOnly) float TargetValueVariance = 0.15f;
    UPROPERTY(EditDefaultsOnly) FGameplayTagContainer ZoneTags;
};
```

Reject invalid rows at generation time: missing definition, non-positive weight, inverted quantity range, or an item footprint larger than the container. Log once per bad row.

### M4 value-budget selection

`UItemDefinition::LootValue` is the static per-unit value. On first open, the Server computes:

```text
target = BaseTargetValue * GameState.LootValueMultiplier * random(1 - variance, 1 + variance)
```

For each roll, rows in the matching pool whose `RequiredZoneTags` are contained by the container's `ZoneTags` participate. Their effective weight is `Weight * clamp(remainingBudget / Item.LootValue, 0.20, 2.0)`. This keeps base table weight authoritative while gently favouring entries appropriate to the remaining value budget. Quantity is sampled from the row range, then first merges into compatible stacks and places remaining stacks row-major; failed placement ends that roll without retry loops. M4's default container is **5 columns x 8 rows**. Future definitions may keep width 5 and choose any height through 10.

## Determinism

Add `RaidSeed` (int32, replicated) to `AMYPROJ2GameState`, assigned by GameMode on Server. Every placed container has an editor-assigned stable `FGuid ContainerId`. Generation uses a local `FRandomStream(Hash(RaidSeed, ContainerId, GenerationVersion))`; never use global `FMath::Rand`.

Determinism is for debugging and host authority, not for client-side generation. Clients never run loot rolls. If all clients already received `RaidSeed`, that does not grant authority.

## Container contract

```cpp
UCLASS()
class ALootContainer : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInventoryComponent> Inventory;
    UPROPERTY(EditInstanceOnly) FGuid ContainerId;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<ULootContainerDefinition> Definition;
    UPROPERTY(ReplicatedUsing=OnRep_Generated) bool bGenerated = false;
    UPROPERTY(ReplicatedUsing=OnRep_OpenState) bool bHasBeenOpened = false;
};
```

The first accepted interaction calls `AuthorityEnsureGenerated()` exactly once and sets `bHasBeenOpened`. `bGenerated`, `bHasBeenOpened`, and inventory entries are durable replicated state, so late joiners reconstruct the container/lid. Active viewer lists and per-player open/close state remain local UI state; container content is replicated to all relevant clients for prototype simplicity.

Use NetDormancy only after correctness: wake/flush before mutation, return dormant after a quiet delay. Do not make a container dormant while a transfer is pending.

## Transfer RPCs

RPCs live on the owned PlayerController:

```cpp
UFUNCTION(Server, Reliable)
void ServerTakeFromContainer(ALootContainer* Container, FGuid ItemId,
                             int32 Quantity, uint16 RequestId);

UFUNCTION(Server, Reliable)
void ServerPutIntoContainer(ALootContainer* Container, FGuid ItemId,
                            int32 Quantity, FIntPoint TargetPosition,
                            bool bRotated, uint16 RequestId);

UFUNCTION(Client, Reliable)
void ClientOpenLootContainer(ALootContainer* Container);
```

Server validation order: request sequence, living Pawn, valid/generated container, distance, unobstructed `InteractionTrace`, source entry/quantity, destination capacity. Use `AuthorityTransferTo` to stage both inventories, validate, then commit both on the game thread. No mutex is required because gameplay RPCs execute serially, but no delegate/RPC may run between source removal and destination insertion.

If the destination cannot fit the requested quantity, reject the entire request in M4. Partial pickup is deferred. Two players racing for the same entry yield one success and one `MissingItem` rejection.

## UI and interaction

Interaction remains handled by `UInteractionComponent`. After Server acceptance, the owning PlayerController receives `ClientOpenLootContainer`. UI shows player and container grids side by side. Closing the UI is local presentation only; walking beyond range causes subsequent transfer requests to fail.

## MCP assets

- `/Game/Raid/Data/Loot/DT_Loot_Test`
- `/Game/Raid/Data/Loot/DA_Container_TestCrate`
- `/Game/Raid/Blueprints/BP_LootContainer_TestCrate`
- `/Game/Raid/UI/WBP_LootContainer` (deferred from this implementation; C++ open notification and data binding surface are present)
- Place two crates with unique `ContainerId` values in `L_Test_Network`.

## Implementation order

1. Add `RaidSeed`, loot rows/definition, and pure deterministic generation tests.
2. Add container Actor and reusable world inventory replication.
3. Integrate `IInteractable` and first-open generation.
4. Implement atomic transfer API and owned Controller RPCs.
5. Create the test DataTable/definition/Blueprint and place two uniquely identified crates. UI is deliberately deferred; `ClientOpenLootContainer` is its integration point.
6. Test late join and two-player race under network emulation.

## Acceptance criteria

- Same seed/container/version produces identical generated entries and placements in automated tests.
- Only Server generates contents; opening simultaneously generates once.
- Both players see the same durable contents and open state.
- Racing for one stack produces no duplication and exactly one authoritative winner.
- Distance/LOS/stale/missing/full-destination requests change neither inventory.
- Late join sees current container contents without replayed Multicasts.
- A container with invalid table rows logs them and still terminates generation without an infinite placement loop.

## Stop conditions

Do not start M5 if transfers are implemented as separate remove/add RPCs, containers generate on clients, `ContainerId` is duplicated, or a late joiner sees an unopened/default inventory after the Server has generated it.
