# Agent Handoff Log

This file is the persistent coordination channel for implementation agents. Read it before editing. Append a dated handoff entry after each task. Do not rewrite previous entries except to correct a factual error and mark the correction.

## Current project state

- UE version: 5.8.
- Starting point: nearly untouched Top Down C++ template with TopDown, Strategy, and TwinStick variants.
- Active implementation: none as of 2026-07-20.
- Architecture/design documents are complete for M0-M2.
- Source control metadata is not present at the workspace root; agents must not assume git commands are available.

## Source of truth order

1. The user's latest instruction.
2. `Docs/technical/network-architecture.md` for network invariants.
3. The active milestone document in `Docs/technical/`.
4. `Docs/technical/network-contracts.json` for exact defaults/names.
5. `Docs/design.md` for product intent and later milestones.
6. Existing template code/assets.

When documents conflict, stop implementation of the conflicting portion, record the conflict here, and follow the higher-priority source. Do not silently invent a third design.

## Required implementation order

```text
M0 network foundation
  -> M0 two-player acceptance
  -> M1 character/interaction
  -> M1 latency acceptance
  -> M2 combat
  -> M2 four-player acceptance
```

An agent should claim only one milestone or one explicitly bounded defect at a time. Do not begin the next milestone to make the current demo look more complete.

## Agent startup checklist

1. Read `Docs/design.md`.
2. Read `Docs/technical/network-architecture.md` completely.
3. Read the active milestone document completely.
4. Read `Docs/technical/network-contracts.json`.
5. Inspect current Source, Config, relevant Content asset paths, and this log.
6. Confirm the previous milestone's acceptance criteria still hold.
7. State planned files and tests before editing.

## Implementation constraints

- Preserve UE native CharacterMovement replication. A custom Transform RPC is a release blocker.
- Server owns gameplay state. Local prediction is presentation/input only.
- C++ defines replicated state, validation, and authority rules. Blueprint/MCP work supplies derived assets, map setup, tuning, and presentation.
- Never call a Server RPC from a client on an unowned world Actor. Route through owned Pawn/Controller/component.
- Do not add OnlineSubsystem, GAS, inventory, loot, SaveGame sync, or later milestone scaffolding during M0-M2 unless the active document requests it.
- Do not modify Strategy/TwinStick variant code as a shortcut. Production classes should derive from the base Top Down path or explicitly replace it.
- MCP output is not self-validating. Save assets, compile, run PIE, and inspect logs.
- Avoid broad refactors and asset renames during network milestones.

## Required handoff format

Append entries using this exact structure:

```markdown
## YYYY-MM-DD HH:MM - AgentName - Mx / task

Status: COMPLETE | PARTIAL | BLOCKED

Changed:
- `path`: concise behavior change

Contracts:
- Added/changed RPC, replicated property, asset name, config key, or `None`.

Validation:
- Command/editor test and exact result.
- PIE topology, latency/loss settings, and observed result.

Known issues:
- Concrete issue, impact, and reproduction; or `None`.

Next:
- Single recommended next task.
```

`COMPLETE` means every acceptance criterion in the active milestone passed. A successful compile alone is `PARTIAL`.

## Architecture handoff

## 2026-07-20 - LeadTechnicalDesigner - architecture / M0-M2 specification

Status: COMPLETE

Changed:
- `Docs/design.md`: corrected player movement authority and reduced committed two-month scope.
- `Docs/technical/design-review.md`: recorded scope/architecture review and risks.
- `Docs/technical/network-architecture.md`: established network authority, lifecycle, RPC, replication, late-join, and test rules.
- `Docs/technical/m0-network-foundation.md`: defined M0 classes/assets/order/acceptance.
- `Docs/technical/m1-character-interaction.md`: defined input, aiming, interaction interface/RPC validation, and acceptance.
- `Docs/technical/m2-combat.md`: defined Hitscan request/result path, health/ammo replication, collision, and acceptance.
- `Docs/technical/network-contracts.json`: fixed shared names and defaults.
- `Docs/chat.md`: created implementation handoff protocol.

Contracts:
- Player movement uses native UE CharacterMovement Server authority with Autonomous Proxy prediction.
- Interaction and fire requests originate on client-owned components and are validated on Server.
- Durable state uses replicated properties; transient shot/impact presentation may use Unreliable Multicast.

Validation:
- Documentation cross-check only. No Source/Config/Content implementation was performed.
- Existing project was inspected as a nearly untouched UE5.8 Top Down template.

Known issues:
- `Docs/design.md` still describes M3-M10 directionally; those milestones do not yet have authoritative technical specifications.
- Local profile persistence across arbitrary Listen Server hosts remains intentionally unresolved.
- Socket-sprite art approach needs a visual spike before production commitment.

Next:
- Implement and fully validate `Docs/technical/m0-network-foundation.md`.

