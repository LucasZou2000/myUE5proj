# Network Architecture

Status: authoritative for M0-M2. If this document conflicts with `Docs/design.md`, this document wins for implementation details.

## 1. Goal and constraints

- 1-4 player PvE co-op using a Listen Server.
- UE native replication, no custom transport and no backend.
- Server owns every state that can affect another player, world state, damage, rewards, or save data.
- Owning client predicts movement and immediate cosmetic feedback only.
- M0-M2 must work in PIE with one Listen Server and one client under packet lag/loss simulation.

## 2. Non-negotiable decisions

1. Keep `ACharacter` and `UCharacterMovementComponent` native movement prediction/correction.
2. Never send player Transform through a project-defined RPC.
3. Client RPC requests express intent, not results. Examples: `ServerTryInteract(Target)` and `ServerFire(Request)`.
4. The Server re-derives or validates range, rate, ownership, ammo, collision, and target state.
5. Persistent state never lives only on Pawn. Use PlayerState for per-player replicated raid state and SaveGame only outside a raid.
6. `GameMode` is Server-only. Replicated match/raid state belongs in `GameState`.
7. Use replicated properties for durable state; RPCs are for transient events. Late joiners must not depend on old Multicasts.
8. Do not use Reliable RPC for continuous input, aim updates, movement, or automatic-fire ticks.

## 3. Framework ownership

| Class | Exists on | Responsibility |
|---|---|---|
| `AMYPROJ2GameMode` | Server only | Login/spawn rules and M0 test flow; later raid orchestration |
| `AMYPROJ2GameState` | Server + clients | Replicated phase and server clock-derived shared state |
| `AMYPROJ2PlayerState` | Server + clients | Player identity, health summary, alive/extracted state; survives Pawn replacement |
| `AMYPROJ2PlayerController` | Server + owning client | Local input, UI, client-targeted messages; owns request RPC entry points only when appropriate |
| `AMYPROJ2Character` | Server + clients | Native movement, aim-facing presentation, interaction/combat components |
| `UInteractionComponent` | Character replicated owner | Local focus query and Server validation of interaction request |
| `UHealthComponent` | Actor replicated owner | Server-only health mutation and replicated health/death state |
| `UCombatComponent` | Character replicated owner | Fire cadence/ammo authority and weapon request routing |
| `AMYPROJ2Weapon` | Server + clients | Weapon configuration/presentation; Server is authoritative for equipped instance |

## 4. Network roles

| Role | May do | Must not do |
|---|---|---|
| Authority | Apply gameplay state and accept/reject requests | Depend on local-only cursor/UI data |
| Autonomous Proxy | Produce input, predict movement, play immediate local cosmetics | Set authoritative health, ammo, hit target, inventory, or Transform |
| Simulated Proxy | Render replicated actors and cosmetics | Run gameplay traces that produce results |

Every mutating method should begin with an authority guard or be callable only from a Server RPC implementation. `OnRep` functions update presentation/UI and never create new authoritative gameplay state.

## 5. Replication policy

### Durable properties

- `GameState.RaidPhase`: replicated with `OnRep_RaidPhase`.
- `PlayerState.HealthSummary`, `bIsAlive`, `bHasExtracted`: replicated; initial M2 may keep exact health on `UHealthComponent`, but one source of truth only.
- `HealthComponent.CurrentHealth`, `bIsDead`: replicated with notifications.
- `CombatComponent.CurrentAmmo`, `EquippedWeapon`: owner-relevant or general replication as specified in M2.
- Aim yaw: replicated only if native actor rotation does not carry the required presentation. Use an `uint16` compressed yaw and an Unreliable Server RPC capped at 15 Hz.

### Transient events

- Shot and impact cosmetics may use Unreliable NetMulticast after the Server accepts a shot.
- Owner rejection/correction uses a Reliable Client RPC because it is rare and state-correcting.
- Death is durable replicated state; a Multicast can supplement it but cannot be the sole signal.

### Relevancy and frequency starting values

Do not optimize before measurement. Initial values:

| Actor | `NetUpdateFrequency` | Notes |
|---|---:|---|
| Player Character | 30 | Native movement replication |
| Enemy Character | 20 | Server AI, simulated proxies |
| Weapon | 10 | Usually attached; consider dormancy later |
| Static interactable | 2 | Dormant after stable state when M4 exists |
| GameState/PlayerState | engine default | Low-frequency durable state |

## 6. RPC rules

Naming convention uses UE-style prefixes and explicit direction:

```cpp
UFUNCTION(Server, Reliable)
void ServerTryInteract(AActor* Target, uint16 ClientSequence);

UFUNCTION(Server, Unreliable)
void ServerUpdateAim(uint16 CompressedYaw);

UFUNCTION(Server, Reliable)
void ServerStartFire(const FFireRequest& Request);

UFUNCTION(Client, Reliable)
void ClientRejectFire(uint16 ClientShotId, EFireRejectReason Reason);

UFUNCTION(NetMulticast, Unreliable)
void MulticastPlayShotFX(const FShotCosmeticEvent& Event);
```

Rules:

- An Actor receiving a Server RPC must be owned by that client. Do not put client-callable Server RPCs on unowned world actors.
- `Target` references are hints. Server verifies validity, distance, line of sight, interface support, and target state.
- Include small monotonic sequence IDs for interaction and fire requests. They help deduplicate, reconcile owner cosmetics, and debug logs.
- Never trust a client-supplied damage number, weapon class, hit actor, ammo count, or arbitrary origin.
- Server may accept a client aim direction, but validates it against character position and a configurable angular tolerance.

## 7. Failure and late-join behavior

- A rejected request changes no gameplay state and sends a concise rejection to the owner only when correction is useful.
- Disconnect destroys the Pawn; PlayerState lifetime follows UE defaults. Rejoin support is out of scope.
- Late join must reconstruct phase, health, death, equipped weapon, and ammo from replicated properties.
- A cosmetic event missed due to packet loss is acceptable. A health/ammo/phase state missed once must arrive later through property replication.

## 8. Logging and test hooks

Create log categories:

```cpp
DECLARE_LOG_CATEGORY_EXTERN(LogMYPROJ2Net, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMYPROJ2Interaction, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMYPROJ2Combat, Log, All);
```

Every Server request rejection logs: request type, owning player, sequence ID, reason, and relevant distance/cooldown value. Avoid per-frame successful-request logs.

Required PIE profiles:

- 2 players: Listen Server + 1 client.
- 4 players: Listen Server + 3 clients before M2 sign-off.
- Emulation: 100 ms RTT, 2% packet loss; then 200 ms RTT, 5% packet loss as a stress pass.

## 9. Explicitly out of scope for M0-M2

- Steam/EOS sessions, matchmaking, invites, reconnection, host migration.
- Dedicated server packaging.
- Gameplay Ability System.
- Client-side rewind / lag compensation.
- Replication Graph or Iris custom filters.
- Inventory, loot, extraction, SaveGame synchronization.
- Custom network serialization beyond compact `USTRUCT` fields defined here.

