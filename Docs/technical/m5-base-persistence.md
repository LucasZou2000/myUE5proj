# M5 Base and Persistence

Depends on: M4 accepted and `gameplay-contracts.json`.

## Objective

Implement one local player profile, persistent stash, two data-driven base modules, atomic upgrade spending, save versioning, and a graybox base screen. M5 is deliberately offline/local. Raid reward settlement is M8.

## Trust boundary

- The base map runs Standalone, not as a Listen Server.
- Each machine owns its local SaveGame profile.
- No backend, cross-host identity, cloud sync, item trading, or anti-tamper guarantee exists.
- M5 must not write raid loot into the stash. M8 will consume a Server-confirmed settlement payload after extraction.

This boundary prevents a local SaveGame API from being mistaken for authoritative multiplayer persistence.

## Required files

```text
Source/MYPROJ2/Persistence/ExtractionSaveGame.h/.cpp
Source/MYPROJ2/Persistence/ProfileSaveTypes.h
Source/MYPROJ2/Persistence/ProfileSubsystem.h/.cpp
Source/MYPROJ2/Base/HideoutModuleDefinition.h/.cpp
Source/MYPROJ2/Base/BaseTypes.h
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
    UPROPERTY(SaveGame) int32 SaveVersion = 1;
    UPROPERTY(SaveGame) FGuid ProfileId;
    UPROPERTY(SaveGame) int64 SaveGeneration = 0;
    UPROPERTY(SaveGame) FSavedInventory Stash;
    UPROPERTY(SaveGame) TArray<FHideoutModuleState> HideoutModules;
};
```

Do not serialize Fast Array replication metadata, Actor pointers, widgets, DataAsset pointers, or transient raid Controllers. Validate every item definition and grid placement after load. Invalid entries move to a recovery list/log and do not crash profile loading.

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
    bool TryUpgradeModule(FPrimaryAssetId ModuleId, EBaseRejectReason& OutReason);
    const UExtractionSaveGame* GetProfile() const;
};
```

Keep one in-memory profile. Load once when entering the base/front-end flow. Save on successful stash mutation/module upgrade and explicit return-to-menu, with a short debounce to avoid a disk write for every drag event.

Before overwriting the primary slot, save/copy the previously valid profile to the backup slot. Increment `SaveGeneration` only after the in-memory transaction succeeds. On primary load failure, try the backup and record `RecoveredBackup`. File encryption/checksum is out of scope.

All future migrations are explicit `Version N -> N+1` functions. Never silently reset an older valid profile because a field was added.

## Hideout definitions

```cpp
USTRUCT(BlueprintType)
struct FItemCost
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FPrimaryAssetId ItemDefinitionId;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FHideoutModuleLevel
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) TArray<FItemCost> Costs;
    UPROPERTY(EditAnywhere) FGameplayTagContainer GrantedFeatures;
    UPROPERTY(EditAnywhere) FIntPoint StashGridSizeOverride = FIntPoint::ZeroValue;
};

UCLASS(BlueprintType)
class UHideoutModuleDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) FName ModuleId;
    UPROPERTY(EditDefaultsOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly) TArray<FHideoutModuleLevel> Levels;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    { return FPrimaryAssetId(TEXT("HideoutModule"), ModuleId); }
};
```

M5 content: `DA_Module_Stash` level 1 expands 10x8 to 12x10; `DA_Module_Workbench` level 1 grants `Feature.WeaponWorkbench` used by M6. No build timers.

## Upgrade transaction

1. Resolve module/current/next level.
2. Validate prerequisites and all item costs without mutation.
3. Validate stash resize can contain every existing footprint.
4. Stage all stack deductions and state change.
5. Commit in memory.
6. Persist primary/backup slots.
7. Notify UI after save result.

If persistence fails, restore the in-memory pre-transaction snapshot and report failure. Costs are never partially consumed.

## MCP assets

- `/Game/Raid/Maps/L_Base_Graybox`
- `/Game/Raid/Data/Base/DA_Module_Stash`
- `/Game/Raid/Data/Base/DA_Module_Workbench`
- `/Game/Raid/UI/WBP_BaseScreen`
- `/Game/Raid/UI/WBP_Stash`

Use a functional full-screen base UI, not a marketing/landing screen. It needs stash grid, module list, current/next level, costs, and upgrade command.

## Implementation order

1. Add save DTOs, version constants, validation, and migration harness.
2. Add ProfileSubsystem with primary/backup load/save.
3. Adapt inventory grid operations for local stash state without replication/UI duplication.
4. Add module definitions and atomic upgrade transactions.
5. Create base graybox map/UI and test cold restart.
6. Corrupt/missing/old-version test passes.

## Acceptance criteria

- Missing profile creates a valid version-1 profile with stable ProfileId.
- Stash positions/quantities/module levels survive editor/game restart.
- Invalid/corrupt primary uses backup or fails explicitly; it never silently overwrites the only recoverable file.
- Insufficient-cost and no-space upgrades consume nothing.
- Successful Stash upgrade expands the grid and persists exactly once per debounced transaction.
- Workbench feature is data-driven and queryable by M6.
- Running M5 base flow does not start network travel or replicate SaveGame data.

## Stop conditions

Do not start M6 if widgets own save state, raw UObject pointers are serialized, failed upgrades consume items, older versions are overwritten without migration, or raid inventories are already merged into stash without M8 settlement authority.
