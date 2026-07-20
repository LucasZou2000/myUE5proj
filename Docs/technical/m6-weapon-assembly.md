# M6 Weapon Assembly

Depends on: M5 accepted, M2 combat remains green, and `gameplay-contracts.json`.

## Objective

Add server-authoritative numerical weapon parts, deterministic derived-stat aggregation, atomic install/remove operations using inventory entries, and one Workbench UI. No part changes weapon visuals in M6.

## Required files

```text
Source/MYPROJ2/Weapons/WeaponPartDefinition.h/.cpp
Source/MYPROJ2/Weapons/WeaponBuildTypes.h
Source/MYPROJ2/Weapons/WeaponStatLibrary.h/.cpp
```

Refactor existing `UWeaponData` to derive from `UItemDefinition` while retaining the existing `DA_Weapon_TestRifle` asset and combat references. Override its Primary Asset type as `Weapon`, preserving `Weapon:TestRifle`. Keep `AMYPROJ2Weapon` as the visual/runtime Actor. `UCombatComponent` owns the authoritative active build and derived stats.

## Definitions and state

```cpp
UENUM(BlueprintType)
enum class EWeaponPartSlot : uint8 { Barrel, Stock, Magazine, Optic };

USTRUCT(BlueprintType)
struct FWeaponStatBlock
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float Damage = 20.0f;
    UPROPERTY(EditAnywhere) float FireIntervalSeconds = 0.2f;
    UPROPERTY(EditAnywhere) float RangeCm = 5000.0f;
    UPROPERTY(EditAnywhere) float SpreadDegrees = 0.0f;
    UPROPERTY(EditAnywhere) float NoiseRadiusCm = 1200.0f;
    UPROPERTY(EditAnywhere) int32 MagazineSize = 12;
};

USTRUCT(BlueprintType)
struct FWeaponStatDelta
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float Damage = 0.0f;
    UPROPERTY(EditAnywhere) float FireIntervalSeconds = 0.0f;
    UPROPERTY(EditAnywhere) float RangeCm = 0.0f;
    UPROPERTY(EditAnywhere) float SpreadDegrees = 0.0f;
    UPROPERTY(EditAnywhere) float NoiseRadiusCm = 0.0f;
    UPROPERTY(EditAnywhere) int32 MagazineSize = 0;
};

USTRUCT(BlueprintType)
struct FWeaponStatScale
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float Damage = 1.0f;
    UPROPERTY(EditAnywhere) float FireIntervalSeconds = 1.0f;
    UPROPERTY(EditAnywhere) float RangeCm = 1.0f;
    UPROPERTY(EditAnywhere) float SpreadDegrees = 1.0f;
    UPROPERTY(EditAnywhere) float NoiseRadiusCm = 1.0f;
    UPROPERTY(EditAnywhere) float MagazineSize = 1.0f;
};

USTRUCT(BlueprintType)
struct FWeaponStatModifier
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FWeaponStatDelta Additive;
    UPROPERTY(EditAnywhere) FWeaponStatScale Multiplicative;
};

UCLASS(BlueprintType)
class UWeaponPartDefinition : public UItemDefinition
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) EWeaponPartSlot Slot;
    UPROPERTY(EditDefaultsOnly) FGameplayTagQuery CompatibleWeaponQuery;
    UPROPERTY(EditDefaultsOnly) FWeaponStatModifier Modifier;
};

USTRUCT(BlueprintType)
struct FInstalledWeaponPart
{
    GENERATED_BODY()
    UPROPERTY() EWeaponPartSlot Slot;
    UPROPERTY() FGuid SourceInstanceId;
    UPROPERTY() FPrimaryAssetId PartDefinitionId;
};
```

`UWeaponData` gains base `FWeaponStatBlock` and weapon compatibility tags. Do not use `TMap` for the replicated build. Use a slot-sorted small `TArray<FInstalledWeaponPart>` so serialization/order is deterministic.

## Stat aggregation

Pure `WeaponStatLibrary::BuildStats(Base, Parts)`:

1. Sort parts by numeric slot enum.
2. Start from base stats. During the existing asset migration, set `FireIntervalSeconds = 60 / FireRate`; do not reinterpret 300 shots/minute as 300 seconds.
3. Sum all additive modifiers.
4. Multiply by all multipliers, whose neutral default is 1.0 for scalar fields.
5. Clamp once using `gameplay-contracts.json` ranges.
6. Round integer magazine size once at the end.

Never aggregate from previously derived stats; repeated equip/unequip must not drift. Add order-independence and round-trip automation tests.

## Combat integration

```cpp
UPROPERTY(ReplicatedUsing=OnRep_InstalledParts)
TArray<FInstalledWeaponPart> InstalledParts;

UPROPERTY(ReplicatedUsing=OnRep_DerivedStats)
FWeaponStatBlock DerivedStats;

UFUNCTION(Server, Reliable)
void ServerInstallPart(FGuid InventoryItemId, EWeaponPartSlot Slot,
                       uint16 RequestId);

UFUNCTION(Server, Reliable)
void ServerRemovePart(EWeaponPartSlot Slot, FIntPoint InventoryPosition,
                      bool bRotated, uint16 RequestId);
```

Only owner needs part/build replication in M6 because there are no remote visuals. The Server always computes `DerivedStats`; the client copy is UI only. Fire validation/trace/damage reads only `DerivedStats`, never DataAsset defaults after a build exists.

Installing validates: Workbench feature in base context or explicit M6 raid-test flag, living owner, inventory entry, quantity one, item type/definition, matching requested slot, compatibility query, and no existing part in slot. It then atomically removes the inventory entry and installs the part.

Removing validates a free destination footprint before changing the build, then atomically creates the same instance in inventory. Preserve `SourceInstanceId` so install/remove cannot duplicate identity.

Magazine behavior: derived `MagazineSize` caps `CurrentAmmo`. Installing/removing never creates ammo; clamp down if capacity shrinks and leave unchanged if it grows. Reload/reserve ammo remains deferred.

## Noise integration

After the Server accepts a shot, report `UAISense_Hearing::ReportNoiseEvent` using derived `NoiseRadiusCm`. M6 can log/test the event; M7 consumes it. Do not report predicted/rejected client shots.

## Persistence

M6 introduces save version 2 only when weapon builds are written to the local base profile. Add an explicit v1->v2 migration that creates an empty build list. Raid build/loadout transfer remains M8. Do not embed raw part DataAssets in SaveGame.

## MCP assets

- `/Game/Raid/Data/Items/DA_Part_TestBarrel`
- `/Game/Raid/Data/Items/DA_Part_TestStock`
- `/Game/Raid/UI/WBP_WeaponWorkbench`
- Update `DA_Weapon_TestRifle` base stat block/tags without replacing its asset path.

## Implementation order

1. Refactor WeaponData inheritance with asset load/compile verification.
2. Add part definitions/build structs and pure aggregation tests.
3. Make CombatComponent consume derived stats with no-part regression equivalence.
4. Add atomic inventory install/remove and request rejection.
5. Add noise event and M7-facing hook.
6. Add Workbench UI/assets and save migration if persistence is enabled now.
7. Run M2 combat and M3 inventory regression tests.

## Acceptance criteria

- Empty build matches original M2 weapon behavior within float tolerance.
- Installing/removing parts changes only declared stats and never duplicates/loses the inventory instance.
- Aggregation is independent of install order and has no repeated-operation drift.
- Incompatible/wrong-slot/missing/full-inventory/stale requests mutate nothing.
- Server damage/rate/range/spread/noise use derived stats; editing the client UI copy has no authority.
- Capacity shrink clamps ammo; capacity growth grants no ammo.
- Accepted shots emit one Server noise event; rejected shots emit none.

## Stop conditions

Do not start M7 if clients can submit modifiers/stat blocks, derived values accumulate incrementally, part removal can fail after the part was already removed, or the original no-part weapon regression is broken.
