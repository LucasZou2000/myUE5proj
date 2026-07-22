# M5 Base and Persistence

Depends on: M4 accepted and `gameplay-contracts.json`.

## Objective

Implement one local player profile, persistent 5x20 stash, current carried inventory, currency, save versioning, and a minimal base screen. This implementation also provides a **Standalone-only** extraction/death settlement bridge so the requested base -> raid -> settle loop can be tested before M8.

## Trust boundary

- The base map runs Standalone, not as a Listen Server.
- Each machine owns its local SaveGame profile.
- No backend, cross-host identity, cloud sync, item trading, or anti-tamper guarantee exists.
- The Server clears raid inventory/currency and sends an owner-only `FRaidSettlementPayload`; only the owning local profile writes that payload to disk. This bridge is supported in Standalone only. A future multiplayer base/deploy flow must use a Server-validated cross-map payload instead of reading another machine's save.

This boundary prevents a local SaveGame API from being mistaken for authoritative multiplayer persistence.

## Required files

```text
Source/MYPROJ2/Persistence/ExtractionSaveGame.h/.cpp
Source/MYPROJ2/Persistence/ProfileSaveTypes.h
Source/MYPROJ2/Persistence/ProfileSubsystem.h/.cpp
Source/MYPROJ2/Base/BaseStashWidget.h/.cpp
```

`UProfileSubsystem : UGameInstanceSubsystem` is the only service allowed to load/save slots. UI and Actors request operations from it; they do not call `UGameplayStatics::SaveGameToSlot` directly.

## Save schema

```cpp
USTRUCT(BlueprintType)
struct FSavedInventory
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FIntPoint GridSize;
    UPROPERTY(SaveGame) TArray<FItemInstance> Items;
};

USTRUCT(BlueprintType)
struct FHideoutModuleState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FPrimaryAssetId ModuleId;
    UPROPERTY(SaveGame) int32 Level = 0;
};

UCLASS()
class UExtractionSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 SaveVersion = 2;
    UPROPERTY(SaveGame) FGuid ProfileId;
    UPROPERTY(SaveGame) int64 SaveGeneration = 0;
    UPROPERTY(SaveGame) FSavedInventory Stash;
    UPROPERTY(SaveGame) FSavedInventory CurrentInventory;
    UPROPERTY(SaveGame) int64 CurrentCurrency;
    UPROPERTY(SaveGame) int64 StashCurrency = 0;
};
```

`Stash.GridSize` is fixed to **5x20**. `CurrentInventory` and `CurrentCurrency` are the player's second and only other gameplay inventory state. They are persisted before level travel and restored after travel; there is no separate preparation/loadout inventory or handoff gameplay object. Do not serialize Fast Array replication metadata, Actor pointers, widgets, DataAsset pointers, or transient raid Controllers. Validate every item definition and grid placement after load; invalid entries are dropped with a log and do not crash profile loading.

## Profile subsystem

```cpp
UENUM(BlueprintType)
enum class EProfileLoadResult : uint8
{ Loaded, CreatedNew, RecoveredBackup, Failed };

UCLASS()
class UProfileSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    EProfileLoadResult LoadOrCreateProfile();
    bool SaveProfile();
    const UExtractionSaveGame* GetProfile() const;
};
```

Keep one in-memory profile. Load once during GameInstance initialization. Save after every successful base transaction and after settlement. This debug slice intentionally writes immediately rather than adding a debounce queue.

Before overwriting the primary slot, save/copy the previously valid profile to the backup slot. Increment `SaveGeneration` only after the in-memory transaction succeeds. On primary load failure, try the backup and record `RecoveredBackup`. File encryption/checksum is out of scope.

All future migrations are explicit `Version N -> N+1` functions. Never silently reset an older valid profile because a field was added.

## Current Base Flow

`UBaseStashWidget` is native and intentionally simple: warehouse summary, current-carry summary, `TAKE ALL`, `STORE ALL`, `ENTER RAID`, and close. It does not own any state. `OpenBaseStash` opens it from the console until a menu binding exists.

`TAKE ALL` / `STORE ALL` stage complete item and currency moves first. If the destination grid cannot hold every stack, neither side changes. `UProfileSubsystem::MoveItem` remains available for later single-stack/drag UI.

`ENTER RAID` saves the current carried inventory/currency, then travels to `L_Test_Network`. The Standalone GameMode restores that same current state into the Server-owned raid inventory. On `DebugExtract` (temporary M8 bridge), Server transfers all carried items and currency into `FRaidSettlementPayload`, clears the raid state, and the owner profile merges it into stash. `UHealthComponent` calls the equivalent death path on zero health, which clears carried items/currency and writes no reward.

The profile's current-carry fields represent the last saved pre-raid state. Base `TAKE ALL` / `STORE ALL` updates them immediately; raid loot and movement are runtime-only. Exiting the process during a raid therefore preserves the pre-raid carry, while death explicitly clears it and extraction settles it into the stash.

## MCP assets

- Native `UBaseStashWidget`; no Widget Blueprint or base map is required for this slice.
- Open with `OpenBaseStash`; use `DebugGrantStashCurrency <amount>` only to test currency before a real reward source exists.

## Implementation order

1. Add save DTOs, version constants, validation, and migration.
2. Add ProfileSubsystem with primary/backup load/save and atomic inventory/currency moves.
3. Add level-travel current-inventory persistence plus Server-owned extraction/death settlement.
4. Create the native debug base screen and validate cold restart.

## Acceptance criteria

- Missing profile creates a valid version-2 profile with stable ProfileId.
- 5x20 stash positions, current-carry positions, quantities, and both currencies survive restart.
- Invalid/corrupt primary uses backup or fails explicitly; it never silently overwrites the only recoverable file.
- Full transfer into an undersized destination changes neither inventory nor currency.
- Extraction merges items/currency into stash once; death clears raid inventory/currency without a stash reward.
- Running the base flow never replicates SaveGame data. The only network data is the Server-confirmed settlement payload.

## Stop conditions

Do not add multiplayer deployment until the base-to-raid payload has an explicit Server trust contract. Do not let widgets own save state, serialize raw UObject pointers, or let failed saves consume/move items or currency.
