# Agent Handoff

This is the compact implementation handoff. Read it before editing. Keep only current facts, unresolved issues, and the most recent handoff per active milestone; do not append long debugging transcripts.

## Source of truth

1. User's latest instruction.
2. Active milestone document under `Docs/technical/`.
3. `Docs/technical/network-architecture.md`.
4. `Docs/technical/network-contracts.json` and `gameplay-contracts.json`.
5. `Docs/design.md`.
6. Existing implementation.

If implementation and documents disagree, report the mismatch before expanding it. Fix narrowly and update this file.

## Current state

Engine: UE 5.8, C++ Top Down project, Listen Server target, 1-4 PvE players.

Completed and user-accepted:

- M0: GameMode/GameState/PlayerState, Listen Server join/spawn, raid phase replication.
- M1: WASD native CharacterMovement, cursor aim, replicated aim yaw, secure interaction request path.
- M2: Server-authoritative Hitscan, health/ammo replication, weapon Actor/muzzle, damage target, host/client firing.
- M2 visual correction: placeholder `SkeletalCube` is approximately 180cm tall inside the 192cm capsule; weapon/muzzle is at waist height. User confirmed it works in PIE.

Authoritative technical specifications now exist for M0-M7. No M3-M7 gameplay implementation exists yet.

## Existing production path

```text
AMYPROJ2GameMode
AMYPROJ2GameState
AMYPROJ2PlayerState
AMYPROJ2PlayerController
AMYPROJ2CharacterBase
  UInteractionComponent
  UCombatComponent
  UHealthComponent
AMYPROJ2Weapon
```

Primary assets/maps:

```text
/Game/Raid/Blueprints/BP_RaidGameMode
/Game/Raid/Blueprints/BP_RaidPlayerController
/Game/Raid/Blueprints/BP_RaidCharacter
/Game/Raid/Blueprints/BP_TestDamageTarget
/Game/Raid/Blueprints/DA_Weapon_TestRifle
/Game/Raid/Maps/L_Test_Network
```

Do not build new gameplay on the template Strategy/TwinStick variants.

## Known technical debt

These are planned prerequisites, not reasons to redesign completed milestones:

- Combat and interaction currently share `ECC_Visibility`. M3 step 0 must add `WeaponTrace` and `InteractionTrace`, migrate responses/traces, and rerun M1/M2 smoke tests.
- `UHealthComponent` currently derives death from `Health <= 0` and lacks durable `bIsDead`/death delegates. M7 owns the explicit death-state upgrade; M3 may add authority-only healing without implementing death orchestration.
- Existing weapon data uses shots-per-minute `FireRate`. M6 migrates combat to `FireIntervalSeconds` inside deterministic derived stats while preserving no-part M2 behavior.
- SaveGame has no multiplayer trust. M5 is local/offline base persistence; M8 will define Server-confirmed raid settlement.

## Required order

```text
M3 Inventory and Items
  -> M3 acceptance + M1/M2 regression
M4 Loot and Containers
  -> M4 two-player race + late-join acceptance
M5 Base and Persistence
  -> cold-restart/backup/migration acceptance
M6 Weapon Assembly
  -> combat/inventory regression
M7 Enemy Spawning and AI
  -> four-player/network/late-join acceptance
```

Do not begin the next milestone until every acceptance criterion and stop condition in the current document has been addressed. A successful compile is `PARTIAL`, not `COMPLETE`.

## Architecture invariants

- Server owns movement result, health, ammo, inventory mutations, loot generation, weapon builds, AI, and rewards.
- Preserve native CharacterMovement prediction/correction. No custom Transform RPC.
- Client RPCs express intent; Server revalidates identity, ownership, distance, LOS, quantity, capacity, cooldown, and life state.
- Durable/late-join state uses replicated properties/Fast Arrays. Multicast is cosmetic only.
- Client-callable Server RPCs live on client-owned Controller/Pawn/components, never unowned containers/enemies.
- Player raid inventory lives on PlayerController; world container inventory follows container relevancy.
- SaveGame contains plain IDs/value structs, never Actor/UObject/widget pointers or replication metadata.
- C++ owns authority/transactions. Blueprint/MCP owns derived assets, tuning, assembly, and presentation.
- MCP work is incomplete until assets are compiled, saved, re-read, and tested in PIE.
- Preserve unrelated user changes; inspect `git status` before editing. The user performs commits manually.

## Milestone documents

```text
Docs/technical/m3-inventory-items.md
Docs/technical/m4-loot-containers.md
Docs/technical/m5-base-persistence.md
Docs/technical/m6-weapon-assembly.md
Docs/technical/m7-enemy-ai.md
Docs/technical/gameplay-contracts.json
```

## Agent startup

1. Read this file, `network-architecture.md`, `gameplay-contracts.json`, and the active milestone document completely.
2. Inspect current Source/Config/assets and `git status`; do not trust old transcript assumptions.
3. Run the previous milestone smoke test before structural changes.
4. State the bounded task, planned files/assets, and validation before editing.
5. Implement only the current milestone; do not scaffold later systems unless its document explicitly reserves a contract.
6. Update this handoff with concise results and remaining work.

## Handoff format

```markdown
## YYYY-MM-DD HH:MM - Agent - Mx / task

Status: COMPLETE | PARTIAL | BLOCKED

Changed:
- `path`: behavior, not implementation narration.

Contracts:
- RPC/replicated property/data asset/config/schema changes, or `None`.

Validation:
- Build/test command and exact result.
- PIE topology + latency/loss + acceptance results.

Remaining:
- Unmet acceptance criteria or `None`.

Next:
- One bounded next task.
```

When a milestone completes, replace its intermediate entries with one final summary instead of retaining every failed attempt.

## 2026-07-21 - Lead Technical - M3-M7 specification handoff

Status: COMPLETE

Changed:

- `Docs/design.md`: M3-M7 direction aligned with the authoritative specifications and reduced scope.
- `Docs/technical/m3-inventory-items.md`: owner-private Fast Array grid inventory, item definitions, transactions, UI, and collision-channel prerequisite.
- `Docs/technical/m4-loot-containers.md`: deterministic Server loot, world container replication, owned transfer RPCs, race/late-join behavior.
- `Docs/technical/m5-base-persistence.md`: local profile/stash, backup/version/migration rules, and atomic base upgrades.
- `Docs/technical/m6-weapon-assembly.md`: numerical parts, deterministic stat aggregation, atomic install/remove, combat/noise integration.
- `Docs/technical/m7-enemy-ai.md`: Server-only Behavior Tree AI, perception, attack/death, spawn director, and network acceptance.
- `Docs/technical/gameplay-contracts.json`: shared names, limits, defaults, and deferred scope.
- `Docs/chat.md`: removed resolved debugging history and established this compact handoff.

Contracts:

- Implementation order is M3 -> M4 -> M5 -> M6 -> M7.
- M3 player inventory lives on PlayerController; M4 container RPCs route through owned PlayerController.
- M5 is offline/local only; raid settlement remains M8.
- M6 clients never submit stat modifiers; M7 AI never runs on clients.

Validation:

- Documentation/JSON consistency checks completed; no M3-M7 Source/Content implementation was performed.
- Existing M0-M2 Source and current MCP-verified asset state were used as the baseline.

Remaining:

- M3 implementation and its collision-channel prerequisite have not started.

Next:

- Implement `Docs/technical/m3-inventory-items.md` from step 1 and stop at M3 acceptance.

## 2026-07-21 - Agent - M3 / implementation and handoff

Status: COMPLETE (pending Lucas's final M3 acceptance PIE)

Changed:

- `Config/DefaultEngine.ini`: added custom trace channels `WeaponTrace` (ECC_GameTraceChannel1) and `InteractionTrace` (ECC_GameTraceChannel2); default profile responses updated so BlockAll/BlockAllDynamic/Pawn/PhysicsActor block WeaponTrace, BlockAll/BlockAllDynamic block InteractionTrace. Added AssetManager `PrimaryAssetTypesToScan` for type `Item` scanning `/Game/Raid/Data/Items`.
- `Source/MYPROJ2/MYPROJ2CollisionChannels.h`: new. `MYPROJ2_TRACE_CHANNEL_WEAPON` / `MYPROJ2_TRACE_CHANNEL_INTERACTION` macros map to ECC_GameTraceChannel1/2.
- `Source/MYPROJ2/Combat/CombatComponent.cpp`: cursor aim + hitscan migrated from `ECC_Visibility` to `MYPROJ2_TRACE_CHANNEL_WEAPON`.
- `Source/MYPROJ2/Interaction/InteractionComponent.cpp`: cursor focus + server LOS migrated from `ECC_Visibility` to `MYPROJ2_TRACE_CHANNEL_INTERACTION`.
- `Source/MYPROJ2/Interaction/TestInteractable.cpp`: mesh now also blocks `InteractionTrace`.
- `Source/MYPROJ2/Combat/TestDamageTarget.cpp`: mesh now also blocks `WeaponTrace`.
- `Source/MYPROJ2/Character/MYPROJ2CharacterBase.cpp`: capsule now also blocks `WeaponTrace`.
- `Source/MYPROJ2/MYPROJ2.Build.cs`: added `GameplayTags` and `NetCore` module dependencies.
- `Source/MYPROJ2/Items/ItemDefinition.h/.cpp`: new. `UItemDefinition : UPrimaryDataAsset` (initially `Abstract`, later fixed) with `ItemId`, `DisplayName`, `Icon`, `ItemType`, `GridSize`, `MaxStack`, `ItemTags`; Primary Asset ID = `Item:<ItemId>`. `UConsumableItemDefinition` adds `HealAmount` for M3 Medkit.
- `Source/MYPROJ2/Inventory/InventoryTypes.h`: new. `FItemInstance` (InstanceId, DefinitionId, Quantity, GridPosition, bRotated). `FReplicatedInventoryEntry : FFastArraySerializerItem` and `FReplicatedInventoryList : FFastArraySerializer` with `NetDeltaSerialize` + `TStructOpsTypeTraits` specialization. `EInventoryRejectReason` enum (includes `FullHealth` per acceptance criterion).
- `Source/MYPROJ2/Inventory/InventoryGridLibrary.h/.cpp`: new. Pure functions: `IsInBounds`, `FootprintsOverlap`, `WouldOverlap`, `FindFirstFit` (deterministic row-major), `ComputeMerge`. No UObject/global lookups.
- `Source/MYPROJ2/Inventory/InventoryComponent.h/.cpp`: new. Server-authoritative `UInventoryComponent` with `ReplicatedItems` Fast Array; authority-only `AuthorityTryAdd/AuthorityTryRemove/AuthorityMoveItem/AuthoritySplitStack/AuthorityMergeStacks/AuthorityUseItem`; RPC contract `ServerMoveItem/ServerSplitStack/ServerMergeStacks/ServerUseItem/ClientInventoryRejected` with `uint16` monotonic request ids and wrap-safe stale-request rejection. Medkit use resolves `UConsumableItemDefinition` and calls `UHealthComponent::AuthorityHeal`. Editor-only `bSeedTestItems`+`SeedItems` grant debug items on Server. Added `GetInventoryDisplayText()`, `GetInventoryEntryStrings()`, `DebugGrantSeedItems()` for UMG data binding.
- `Source/MYPROJ2/Combat/HealthComponent.h/.cpp`: added authority-only `AuthorityHeal(float)` clamped to `MaxHealth`; refuses to heal when dead.
- `Source/MYPROJ2/MYPROJ2PlayerController.h/.cpp`: now owns a `UInventoryComponent` subobject created in the constructor. Controller exists only on Server + owning client, so Fast Array is private without per-connection filters.
- `Source/MYPROJ2/Character/MYPROJ2CharacterBase.h/.cpp`: added `InventoryAction` input binding and `ToggleInventoryUI()` (lazy-creates `WBP_InventoryGrid` on first I-key press).

Assets created (MCP + manual):

- `/Game/Raid/Data/Items/DA_Item_Scrap` (1x1, MaxStack=20, Material)
- `/Game/Raid/Data/Items/DA_Item_Medkit` (2x1, MaxStack=3, Consumable, HealAmount=35)
- `/Game/Raid/Input/IA_Inventory` (Digital)
- `/Game/Raid/UI/WBP_InventoryGrid` (Border + VerticalBox + TextBlock binding to `GetInventoryDisplayText`)
- `IMC_Raid` updated with `IA_Inventory` -> `I` key mapping
- `BP_RaidCharacter` updated with `InventoryAction` property assigned

Contracts:

- `EInventoryRejectReason` adds `FullHealth` (not in the spec's enum list) so Medkit use on a full-health pawn is rejected without state mutation, per the acceptance criterion "full-health Medkit use does not mutate state."
- `UInventoryComponent` lives on `AMYPROJ2PlayerController` per the M3 architecture decision; same component is reusable on world containers in M4.
- `PrimaryAssetTypesToScan` registers type `Item` at `/Game/Raid/Data/Items`. `UItemDefinition::GetPrimaryAssetId()` returns `Item:<ItemId>`.
- Request-id dedupe is monotonic `uint16` with wrap-safe comparison (distance in [-32768, 32767]).
- `UItemDefinition` was initially marked `Abstract`, which prevented `DA_Item_Scrap` from saving; fixed by removing `Abstract` and recompiling.

Validation:

- `read_lints` on Source/MYPROJ2: 0 diagnostics.
- `Build.bat MYPROJ2Editor Win64 Development -waitmutex`: SUCCEEDED (multiple times; fixes included template lambda deduction, TObjectPtr ambiguity, `Abstract` removal).
- MCP asset creation: `DA_Item_Scrap`, `DA_Item_Medkit` created and saved under `/Game/Raid/Data/Items` (Scrap initially failed due to `Abstract`, succeeded after fix).
- Lucas manually wired `IA_Inventory` -> `I` key in `IMC_Raid`, assigned `InventoryAction` in `BP_RaidCharacter`, built `WBP_InventoryGrid` UI with Binding, and verified M0/M1/M2 still work + inventory UI toggles on/off.
- UI displays inventory contents via `GetInventoryDisplayText()` binding.

Remaining (M3 acceptance, pending Lucas's PIE):

- AssetManager scan verification: `?x?` should become `1x1`/`2x1` after Editor restart.
- SeedItems population: `BP_RaidPlayerController` -> `Inventory Component` -> `bSeedTestItems` + `SeedItems` should contain both `DA_Item_Scrap` and `DA_Item_Medkit`.
- Two-player PIE: owner privacy (Host/Client see own inventory), Medkit use (heal + consume), FullHealth rejection, M1/M2 regression.
- Fast Array delta verification (optional technical check).

Next:

- Lucas runs M3 acceptance PIE per checklist above. If all pass, M3 is done and we proceed to M4 (Loot Containers).

## 2026-07-22  - Agent - M4 / container generation and transfer implementation

Status: PARTIAL (functional one-container slice and cold build complete; manual two-player race/late-join acceptance remains).

Changed:

- `Docs/technical/m4-loot-containers.md`: default container grid corrected to 5x8; specifies the target-value budget, map multiplier, zone-tag filtering, budget-aware weights, deterministic placement, and native two-grid UI.
- `Docs/technical/gameplay-contracts.json`: `containerGrid` is now 5 columns x 8 rows.
- `Source/MYPROJ2/Items/ItemDefinition.h`: adds static `LootValue` used only as a generation budget input.
- `Source/MYPROJ2/Framework/MYPROJ2GameState.h/.cpp`, `MYPROJ2GameMode.cpp`: replicated per-raid `RaidSeed` and map-controlled `LootValueMultiplier`; GameMode assigns the seed once per hosted raid.
- `Source/MYPROJ2/Loot/LootTypes.h`: DataTable row contract for pool, item ID, weight, quantities, and required zone tags.
- `Source/MYPROJ2/Loot/LootContainerDefinition.h/.cpp`: Primary Data Asset for pool/table/5x8 grid/value lower bound/zone tags, incremental-attempt cap, and reserved empty cells. Legacy rolls/variance fields remain only for existing assets.
- `Source/MYPROJ2/Loot/LootGenerationLibrary.h/.cpp`: Server-only deterministic local-stream generator. It validates rows, derives a seed from raid/container/version, incrementally places weighted normal items until the map-multiplied value lower bound is reached, preserves empty cells, and uses 1x1 value-gap candidates only when normal shapes cannot complete the target.
- `Source/MYPROJ2/Loot/LootContainer.h/.cpp`: replicated world Actor with 5x8 inventory, stable editor container ID, immediate first-open generation, durable generated/open state, and `IInteractable` integration.
- `Source/MYPROJ2/Inventory/InventoryComponent.h/.cpp`: `AuthorityTransferTo` stages the destination then commits both inventories atomically; no delegate is emitted between source and destination commit.
- `Source/MYPROJ2/MYPROJ2PlayerController.h/.cpp`, `InteractionComponent.cpp`: after local interaction input, a 0.2s presentation delay opens the existing player inventory only if the Server success RPC arrives by the deadline. The success RPC is emitted after immediate server generation; it intentionally does not wait on container inventory replication. No key suppression is implemented yet.
- Current debug presentation: container generation logs every generated item (name, quantity, grid coordinate, value) through `LogMYPROJ2LootContainer`; successful interaction opens the existing `WBP_InventoryGrid`. Container search/two-grid UI is deferred.
- `Source/MYPROJ2/Loot/LootGenerationTests.cpp`: deterministic seed, stable IDs/placement, 5x8 bounds, and invalid-row termination automation coverage.
- `/Game/Raid/Data/Loot/DT_Loot_Test`: Scrap weight 70 (quantity 3-8), Medkit weight 30 (quantity 1-2).
- `/Game/Raid/Data/Loot/DA_Container_TestCrate`: pool `TestCrate`, 4 rolls, 5x8, target value 180, variance 0.15.
- `/Game/Raid/Blueprints/BP_LootContainer_TestCrate`: replicated crate Blueprint with engine cube placeholder mesh and the test definition.
- `L_Test_Network` external actor: one crate at `(300,0,260)`, ContainerId `BF7115F7-484E-022B-3D0D-B39317E64ECE`.
- `DA_Item_Scrap` / `DA_Item_Medkit`: LootValue 15 / 90.

Contracts:

- `ALootContainer` has no client-callable Server RPC. All mutations route through the owning `AMYPROJ2PlayerController`.
- Generation is Server-only and `bGenerated`/`bHasBeenOpened` plus Fast Array inventory are durable replication state for late joins.
- The native widget is presentation only. It submits intent and never predicts inventory mutation.

Validation:

- MCP Live Coding compiled all M4 Source successfully after UE 5.8 shadow-warning fixes. The editor-closed `build.ps1` cold build also completed with `Result: Succeeded` after the incremental-generator and client-delay changes.
- Automation `MYPROJ2.Loot.Generation.DeterminismAndBounds`: Success, 1 passed / 0 failed. The expected invalid-row warning was emitted once.
- MCP-created assets were compiled, saved, and read back. The placed crate reports the expected definition, unique ID, generation version 1, and `bReplicates=true`.
- Two-player PIE (Listen Server + Client 1) started and loaded `L_Test_Network` without M4 errors, then stopped cleanly. No network race or late-join interaction was automated.

Remaining:

- Manually verify full-stack take/put, simultaneous take winner, distance/LOS rejection, and late join. The MCP Slate tool cannot reliably aim at a 3D world actor.

Validated by Lucas (2026-07-22):

- Container interaction executes on the Server, the client presentation delay is correct, and the existing inventory UI opens correctly after the delay.
- PIE two-player shutdown log contains only expected world/network cleanup (`HostClosedConnection` / `Cleanup`) and no M4 error.
- Container searching and the two-grid container UI remain intentionally deferred; generated contents are inspected through `LogMYPROJ2LootContainer`.

Next:

- Run the bounded two-player manual M4 acceptance pass; do not start M5 until it passes.

## 2026-07-22 - Agent - M5 / local stash, currency, and settlement bridge

Status: PARTIAL (implementation and Live Coding compile complete; Standalone PIE persistence/settlement acceptance pending).

Changed:

- `Source/MYPROJ2/Persistence/ExtractionSaveGame.h`, `ProfileSaveTypes.h`, `ProfileSubsystem.h/.cpp`: version-2 local Profile with a fixed 5x20 stash, separate stash/loadout currency, prepared loadout, pending raid handoff, primary/backup save slots, migration, load validation, and in-memory rollback when a save fails.
- `Source/MYPROJ2/Base/BaseStashWidget.h/.cpp`: native debug preparation UI showing warehouse/loadout summaries and `TAKE ALL`, `STORE ALL`, `ENTER RAID` buttons. No grid UI is implemented yet.
- `Source/MYPROJ2/Inventory/InventoryComponent.h/.cpp`: Server-only validated `AuthorityReplaceAll` and `AuthorityClearAll` used only for profile-to-raid handoff and settlement.
- `Source/MYPROJ2/MYPROJ2PlayerController.h/.cpp`: owner-only carried currency, `OpenBaseStash`, temporary `DebugExtract`, settlement client RPC, and `DebugGrantStashCurrency <amount>` for validating currency before a reward source exists.
- `Source/MYPROJ2/MYPROJ2GameMode.h/.cpp`, `HealthComponent.cpp`: Server handles extraction/death by clearing the raid inventory/currency, setting life state, and sending a settlement payload. In Standalone only, `PostLogin` consumes the pending prepared loadout into the Server-owned raid inventory.
- `Docs/technical/gameplay-contracts.json`: warehouse contract changed to 5x20 and save version to 2.
- `Docs/technical/m5-base-persistence.md`: documents this local M5 settlement bridge; M8 remains responsible for real multiplayer extraction/deploy authority.

Contracts:

- SaveGame is local only. UI never calls `SaveGameToSlot` directly and never mutates profile structs itself.
- The Server remains authoritative for live raid inventory and carried currency. Only its settlement RPC authorizes local profile merge.
- The base-to-raid handoff is intentionally limited to Standalone. Do not use it for Listen Server clients until M8 has a cross-host payload/trust contract.
- `TAKE ALL` and `STORE ALL` are all-or-nothing. Later UI can call the existing `MoveItem` method for individual stack movement.

Validation:

- UE MCP Live Coding: `Result: Succeeded` after all M5 C++ files, UHT-generated types, rollback logic, and debug commands were added.
- Base UI lifecycle fix: native `UBaseStashWidget` now creates its WidgetTree in `RebuildWidget`, before Slate creates the root. The prior `NativeConstruct`-only construction produced an empty Slate widget even though `OpenBaseStash` executed. Cold build after this fix: `Result: Succeeded`.

Manual Standalone PIE acceptance:

1. In the PIE console, run `DebugGrantStashCurrency 100`, then `OpenBaseStash`.
2. Click `TAKE ALL`; verify the UI and `LogMYPROJ2Profile` show currency moved from warehouse to loadout.
3. Click `ENTER RAID`; this travels to `L_Test_Network` and applies the prepared loadout to the Server-owned player inventory.
4. Run `DebugExtract`; verify carried currency returns to warehouse, carried items are logged as settled, and the base UI opens again.
5. Repeat with a carried item/currency, then run `DebugKillRaid`; verify the player inventory/currency are cleared and no reward is written to warehouse.
6. Restart the editor and open the base UI again; verify warehouse stacks and currency survived. Save files are `Saved/SaveGames/MYPROJ2_Profile_0.sav` and backup.
