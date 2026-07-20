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
