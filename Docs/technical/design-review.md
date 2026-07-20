# Design Review

Date: 2026-07-20

## Executive decision

The project concept is viable as a small co-op extraction vertical slice. The core loop has a clear risk/reward decision and maps well to UE's authoritative Actor model. The original full feature list is not viable at consistent quality in two months for one developer. The correct target is a complete graybox slice with one example of each core system, then horizontal content only if time remains.

One architectural change is mandatory: do not replace UE CharacterMovement replication with client-authored Transform snapshots. Native movement prediction is both safer and cheaper to maintain. This decision is now reflected in `design.md` and `network-architecture.md`.

## What should remain

- 1-4 player Listen Server PvE.
- Fixed hand-authored map with authored spawn sockets/markers.
- Server-authoritative enemies, damage, loot, extraction, and settlement.
- Hitscan first; projectiles later only if a specific weapon requires them.
- Data-driven static definitions separated from runtime instance state.
- Local SaveGame for host-owned progression in the prototype.
- Graybox-first milestone order and multiplayer testing at every milestone.

## What should change

### 1. Scope: build one deep slice, not every listed system

M0-M4 plus minimal enemy/extraction integration is the realistic two-month target. A robust one-weapon, one-enemy, one-container, one-extraction flow demonstrates more engineering ability than unfinished base building, attachment slots, multiple extraction types, bosses, and a bespoke character art pipeline.

### 2. Movement: native Server authority

The original low-frequency client Transform broadcast would require custom smoothing, collision reconciliation, teleport protection, listen-host parity handling, and interaction disagreement fixes. UE already solves the required baseline through CharacterMovement. Keep it.

### 3. Persistence: explicitly define the host-save limitation

Without a backend, the host is effectively the source of raid truth. For a portfolio prototype this is acceptable, but client rewards cannot be made trustworthy or portable across arbitrary hosts. The first implementation should return each local player's settlement payload to their own SaveGame after Server-confirmed extraction. Security and cross-host identity are out of scope and must be stated in UI/documentation later.

### 4. Map: use World Partition carefully

A fixed large map plus 1-4 players can use World Partition, but random camps should be authored templates toggled by the Server. Do not build procedural terrain. Before production map work, profile AI navigation rebuild and replication relevancy on a representative graybox; dynamic NavMesh across an unnecessarily large world can dominate iteration time.

### 5. AI: postpone the full behavior tree promise

First enemy needs only idle/patrol, acquire, chase, attack, lose target, death. Reinforcement calls, alert escalation, elites, and bosses are content layers. Validate Server-only AI and remote presentation before expanding the tree.

### 6. Art: prove the rendering idea in a spike

Socket-mounted sprite planes are unusual for readable top-down animation and can suffer from sorting, shadows, depth intersection, and view-angle artifacts. Run a one-character visual spike before committing the whole asset pipeline. Keep the gameplay skeleton independent so a standard skeletal mesh can replace it without touching combat or networking.

### 7. Data: prefer Primary Data Assets before DataTables everywhere

Use `UPrimaryDataAsset` for polymorphic definitions such as weapons/items that reference assets. Use DataTables for large homogeneous tuning rows such as weighted loot entries. Runtime state should store stable IDs plus mutable fields, never a copied asset definition.

## Recommended delivery sequence

| Week | Outcome |
|---|---|
| 1 | M0 multiplayer framework and repeatable PIE test map |
| 2 | M1 movement, aim, interaction, latency validation |
| 3 | M2 combat, health, death, one test target/enemy |
| 4 | M3 inventory with one replicated container workflow |
| 5 | M4 loot generation and pickup race handling |
| 6 | Minimal AI plus fixed extraction and settlement payload |
| 7 | SaveGame/base screen and one art-pipeline spike |
| 8 | Bug fixing, network stress, packaging, presentation |

This schedule has no slack for M5/M6 as originally described. Those systems become stretch work.

## Cross-cutting rules for future design

- Every feature specification states authority, owner, durable replicated state, transient events, late-join behavior, and failure behavior.
- Every milestone has a two-player latency test before the next begins.
- C++ owns network/state invariants; Blueprints own tuning, content assembly, and presentation.
- Do not introduce a framework until two implemented cases prove the shared abstraction.
- Do not optimize bandwidth from estimates. Use network profiler/Insights after the functional path is correct.

## Open decisions, intentionally deferred

- Whether downed/revive exists. `DownedReserved` keeps enum compatibility but no behavior is promised.
- Whether clients write their own Server-confirmed settlement to local SaveGame or only the host profile persists during early tests.
- Whether final sessions use LAN OnlineSubsystem, EOS, or direct travel. M0 uses PIE/direct Listen Server only.
- Whether character visuals remain socket sprites after the M9 spike.

