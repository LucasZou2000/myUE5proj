# M0 Network Foundation

Depends on: `network-architecture.md`.

## Objective

Produce a clean C++ multiplayer skeleton where 1-4 PIE players join a Listen Server, spawn replicated Pawns, observe join/leave state, and expose network diagnostics. M0 contains no combat, inventory, loot, AI, save, or online service.

## Required classes

### New

```text
Source/MYPROJ2/Framework/MYPROJ2GameState.h/.cpp
Source/MYPROJ2/Framework/MYPROJ2PlayerState.h/.cpp
Source/MYPROJ2/Network/MYPROJ2NetworkTypes.h
Source/MYPROJ2/Network/MYPROJ2NetworkSettings.h/.cpp
```

Existing template classes may be moved only if redirects and Blueprint parents are preserved. Prefer leaving them in place during M0.

### `AMYPROJ2GameState`

```cpp
UENUM(BlueprintType)
enum class ERaidPhase : uint8
{
    Lobby,
    Deploying,
    InRaid,
    Resolving,
    Returning
};

UPROPERTY(ReplicatedUsing=OnRep_RaidPhase, BlueprintReadOnly)
ERaidPhase RaidPhase = ERaidPhase::Lobby;

UPROPERTY(Replicated, BlueprintReadOnly)
double PhaseEndServerTime = 0.0;
```

Only `GameMode` may call the authority-only phase setter. Clients calculate countdown with `GetServerWorldTimeSeconds()` and never replicate a ticking seconds value.

### `AMYPROJ2PlayerState`

```cpp
UPROPERTY(Replicated, BlueprintReadOnly)
FGuid RaidPlayerId;

UPROPERTY(ReplicatedUsing=OnRep_LifeState, BlueprintReadOnly)
EPlayerLifeState LifeState = EPlayerLifeState::Alive;
```

`EPlayerLifeState`: `Alive`, `DownedReserved`, `Dead`, `Extracted`. M0 only uses `Alive`; reserved entries prevent later renaming churn. `RaidPlayerId` is assigned on Server and is not a platform account ID.

### `UMYPROJ2NetworkSettings`

Developer settings, not replicated:

```cpp
UPROPERTY(Config, EditAnywhere)
float AimSendRateHz = 15.0f;

UPROPERTY(Config, EditAnywhere)
float MaxInteractDistance = 250.0f;

UPROPERTY(Config, EditAnywhere)
float MaxFireOriginError = 150.0f;

UPROPERTY(Config, EditAnywhere)
float MaxAimErrorDegrees = 35.0f;
```

All network tolerances live here instead of scattered literals.

## Existing class changes

- `AMYPROJ2GameMode`: set `GameStateClass` and `PlayerStateClass`; implement authority-only initial phase assignment and basic `PostLogin`/`Logout` logging.
- `AMYPROJ2Character`: set replication explicitly, preserve native CharacterMovement networking, and remove point-and-click-specific assumptions only when M1 input is added.
- `AMYPROJ2PlayerController`: no session code. Keep local input initialization isolated behind `IsLocalPlayerController()`.
- `MYPROJ2.Build.cs`: no OnlineSubsystem dependency in M0. Add `DeveloperSettings` only if required by the UE5.8 headers used for `UDeveloperSettings`.

## Assets/config

- Create `BP_RaidGameMode` derived from `AMYPROJ2GameMode`.
- Create or repoint a graybox map `L_Test_Network` with at least four `PlayerStart` actors.
- Project default map may stay unchanged until the test map is verified.
- PIE must use `Play As Listen Server`, 2 players, separate process optional.

MCP may create Blueprint/map assets, but C++ names and paths must match this document. MCP-created assets must be saved and then manually verified in PIE.

## Implementation order

1. Add enums/settings/log categories.
2. Add GameState and PlayerState with replication registration.
3. Wire classes in GameMode and Blueprint defaults.
4. Add a four-spawn graybox test map.
5. Validate join, spawn, movement visibility, phase replication, and disconnect.
6. Run emulated latency/loss pass.

## Acceptance criteria

- Client possesses exactly one Character and sees the host move; host sees the client move.
- No custom Transform RPC exists anywhere in Source.
- `RaidPhase` changed on Server is observed once per change by all clients.
- A joining client sees the current durable phase without requiring a Multicast replay.
- Leaving client removes its Pawn and PlayerState from active arrays without errors.
- Output log has no RPC ownership warning, replication warning, or Blueprint parent error.
- M0 agent updates `Docs/chat.md` with files changed, tests run, failures, and next handoff.

## Stop conditions

Do not start M1 if native remote movement is jittery in a zero-lag two-player PIE run, class defaults still point at template Strategy/TwinStick variants, or the GameState/PlayerState classes are not actually active at runtime.

