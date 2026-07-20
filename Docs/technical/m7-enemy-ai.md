# M7 Enemy Spawning and AI

Depends on: M6 accepted, `network-architecture.md`, and `gameplay-contracts.json`.

## Objective

Implement one Server-only enemy simulation with spawn cap, patrol, sight/hearing acquisition, chase, melee attack, target loss, health/death, and replicated presentation state. This milestone proves the full PvE network path; elites, bosses, and new reinforcement spawns are deferred.

## Required files

```text
Source/MYPROJ2/AI/EnemyDefinition.h/.cpp
Source/MYPROJ2/AI/EnemyTypes.h
Source/MYPROJ2/AI/EnemyCharacter.h/.cpp
Source/MYPROJ2/AI/EnemyAIController.h/.cpp
Source/MYPROJ2/AI/EnemySpawnPoint.h/.cpp
Source/MYPROJ2/AI/EnemySpawnDirector.h/.cpp
Source/MYPROJ2/AI/BTTask_EnemyAttack.h/.cpp
Source/MYPROJ2/AI/BTTask_FindPatrolPoint.h/.cpp
```

Use UE Behavior Tree + Blackboard + AI Perception. Do not reuse or modify template TwinStick AI classes.

## Types and definitions

```cpp
UENUM(BlueprintType)
enum class EEnemyAlertState : uint8 { Idle, Suspicious, Alerted, Attacking, Dead };

UCLASS(BlueprintType)
class UEnemyDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly) FName EnemyId;
    UPROPERTY(EditDefaultsOnly) TSubclassOf<AEnemyCharacter> EnemyClass;
    UPROPERTY(EditDefaultsOnly) float MaxHealth = 100.0f;
    UPROPERTY(EditDefaultsOnly) float MoveSpeed = 350.0f;
    UPROPERTY(EditDefaultsOnly) float AttackRangeCm = 180.0f;
    UPROPERTY(EditDefaultsOnly) float AttackCooldownSeconds = 1.2f;
    UPROPERTY(EditDefaultsOnly) float AttackDamage = 15.0f;
    UPROPERTY(EditDefaultsOnly) FGameplayTagContainer EnemyTags;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    { return FPrimaryAssetId(TEXT("Enemy"), EnemyId); }
};

USTRUCT(BlueprintType)
struct FEnemySpawnRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FPrimaryAssetId EnemyDefinitionId;
    UPROPERTY(EditAnywhere) FGameplayTagQuery ZoneQuery;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0")) float Weight = 1.0f;
};
```

Static definition values are resolved on Server. Clients receive replicated runtime outcomes, not a replicated copy of the DataAsset.

## Enemy Character

`AEnemyCharacter : ACharacter`:

- `bReplicates=true`, native replicated movement, Server possesses/runs movement.
- `UHealthComponent` extended with `MaxHealth`, `CurrentHealth`, `bIsDead`, authority-only damage/heal, and death delegate.
- `EEnemyAlertState AlertState` replicated using OnRep for presentation.
- No tick-based client AI, traces, perception, or damage.
- On Server death: set Dead once, stop movement/BT, clear focus, disable gameplay collision, schedule optional corpse dormancy/despawn.
- Death state is a property, not Multicast-only, so late join works.

Update player health integration to use the same component semantics and set `AMYPROJ2PlayerState::LifeState=Dead` on the first zero-health transition. Downed/revive remains unimplemented.

## Controller and Blackboard

`AEnemyAIController` owns `UAIPerceptionComponent` with Sight and Hearing. Perception delegates execute on Server only.

Required Blackboard keys, exact names/types:

```text
TargetActor       Object(Actor)
LastKnownLocation Vector
PatrolLocation    Vector
AlertState        Enum(EEnemyAlertState)
HasLineOfSight    Bool
```

Required tree:

```text
Root Selector
  Dead guard -> stop
  Target valid Sequence
    MoveTo TargetActor
    In attack range decorator
    BTTask_EnemyAttack (cooldown enforced on Server)
  Suspicious Sequence
    MoveTo LastKnownLocation
    wait/search
    clear target after TargetForgetSeconds
  Patrol Sequence
    BTTask_FindPatrolPoint
    MoveTo PatrolLocation
    Wait
```

Perception chooses the nearest alive player with deterministic distance then RaidPlayerId tie-break. Hearing from accepted M6 shots sets LastKnownLocation/Suspicious; direct sight sets TargetActor/Alerted. Nearby existing enemies may receive an alert stimulus as an optional final M7 task; spawning reinforcements is not required.

## Attack contract

`BTTask_EnemyAttack` calls an authority-only method. Validate target alive, authoritative distance, cooldown, and optional LOS immediately before applying damage. Animation timing may be immediate for graybox M7; later notify-based hit frames can replace presentation without changing authority.

No Client/Server RPC is required for enemy attacks because both AI decision and damage execute on Server. Cosmetic attack/hit events may use Unreliable Multicast, while health/death remains replicated state.

## Spawn system

`AEnemySpawnPoint` has zone tags, radius, and enabled flag. `AEnemySpawnDirector` exists on Server and owns:

- spawn table/definitions;
- deterministic `FRandomStream` seeded from GameState RaidSeed;
- weak set of living enemies;
- max alive and spawn interval;
- minimum distance/visibility checks from alive players.

Clients never spawn enemies. Director binds death/end-play to maintain count; it does not scan all Actors every tick. Start with `MaxAlive=8`. Do not despawn an enemy that is alerted, visible, recently damaged, or within the configured player distance.

## MCP assets

- `/Game/Raid/Data/AI/DA_Enemy_Grunt`
- `/Game/Raid/Data/AI/DT_EnemySpawn_Test`
- `/Game/Raid/AI/BB_Enemy_Grunt`
- `/Game/Raid/AI/BT_Enemy_Grunt`
- `/Game/Raid/Blueprints/BP_Enemy_Grunt`
- `/Game/Raid/Blueprints/BP_EnemySpawnDirector`
- At least four tagged `AEnemySpawnPoint` actors and a NavMeshBoundsVolume in `L_Test_Network`.

MCP must compile/save assets and inspect Blackboard key types. Run navmesh visualization once; AI acceptance cannot rely on asset creation alone.

## Implementation order

1. Normalize HealthComponent death semantics and rerun M2 damage regression.
2. Add EnemyDefinition/Character with replicated movement, health, alert, death.
3. Add AIController perception and deterministic target selection.
4. Build Blackboard/Behavior Tree/tasks and validate one placed enemy.
5. Connect M6 shot noise and player damage.
6. Add spawn point/director with cap and deterministic selection.
7. Run two-/four-player network, late-join, and server-only AI checks.

## Acceptance criteria

- Exactly one Server AI simulation controls each enemy; clients have no AIController/BT execution.
- Patrol, sight acquire, hearing investigate, chase, attack cooldown, target loss, reacquire, and death all execute.
- Enemy and player health converge; death happens once and late join sees dead state.
- Enemy attacks cannot hit out of range, through invalid LOS when required, or after target/enemy death.
- Accepted noisy weapon shots alert enemies; rejected/dry fire does not.
- Spawn cap is never exceeded under simultaneous deaths/spawns; clients spawn none.
- Four-player PIE at 100ms RTT/2% loss has no RPC ownership/replication warnings and remote enemy movement is readable.
- Destroying enemies/director during PIE leaves no dangling timers/delegates.

## Stop conditions

Do not start M8 if AI runs on clients, Blackboard state is replicated wholesale, death depends only on Multicast, spawn count can drift, or enemy damage bypasses the shared authority-only HealthComponent path.
