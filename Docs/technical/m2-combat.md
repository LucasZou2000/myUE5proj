# M2 Combat

Depends on: M1 accepted and `network-architecture.md`.

## Objective

Implement one server-authoritative semi-automatic Hitscan weapon, health/death state, immediate local shot feedback, replicated ammo, and shared impact/death presentation. M2 proves the combat network path; it is not a general weapon framework.

## Required classes

```text
Source/MYPROJ2/Combat/CombatTypes.h
Source/MYPROJ2/Combat/CombatComponent.h/.cpp
Source/MYPROJ2/Combat/HealthComponent.h/.cpp
Source/MYPROJ2/Combat/MYPROJ2Weapon.h/.cpp
Source/MYPROJ2/Combat/WeaponData.h
Source/MYPROJ2/Combat/TestDamageTarget.h/.cpp
```

Use one `UPrimaryDataAsset` weapon definition. Do not add attachments, rarity, reload inventory, projectiles, recoil simulation, GAS, or status effects.

## Data definitions

```cpp
UCLASS(BlueprintType)
class UWeaponData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FPrimaryAssetId WeaponId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1.0"))
    float Damage = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.1"))
    float FireInterval = 0.2f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="100.0"))
    float Range = 5000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1"))
    int32 MagazineSize = 12;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;
};

USTRUCT()
struct FFireRequest
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 ClientShotId = 0;

    UPROPERTY()
    FVector_NetQuantizeNormal AimDirection = FVector::ForwardVector;

    UPROPERTY()
    float ClientFireTime = 0.0f; // diagnostics only; not trusted for rewind
};

USTRUCT()
struct FShotCosmeticEvent
{
    GENERATED_BODY()

    UPROPERTY()
    uint16 ServerShotId = 0;

    UPROPERTY()
    FVector_NetQuantize MuzzleLocation;

    UPROPERTY()
    FVector_NetQuantize ImpactLocation;

    UPROPERTY()
    FVector_NetQuantizeNormal ImpactNormal;

    UPROPERTY()
    uint8 bBlockingHit : 1;
};
```

Do not put damage or hit Actor in `FFireRequest`. The Server derives both.

## `UHealthComponent`

```cpp
UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly)
float MaxHealth = 100.0f;

UPROPERTY(ReplicatedUsing=OnRep_CurrentHealth, BlueprintReadOnly)
float CurrentHealth = 100.0f;

UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly)
bool bIsDead = false;
```

- `ApplyDamage` is authority-only.
- Clamp health to `[0, MaxHealth]`.
- The first transition to zero sets death once. Duplicate damage cannot emit duplicate death gameplay.
- `OnRep` drives UI/presentation. Listen Server calls the same presentation helpers after mutation.
- For player characters, update PlayerState life state on Server when death occurs. Do not maintain two independent health values.

## `UCombatComponent`

```cpp
UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon)
TObjectPtr<AMYPROJ2Weapon> EquippedWeapon;

UPROPERTY(ReplicatedUsing=OnRep_CurrentAmmo)
int32 CurrentAmmo = 0;

UFUNCTION(Server, Reliable)
void ServerFire(const FFireRequest& Request);

UFUNCTION(Client, Reliable)
void ClientRejectFire(uint16 ClientShotId, EFireRejectReason Reason,
                      int32 AuthoritativeAmmo);

UFUNCTION(NetMulticast, Unreliable)
void MulticastShotFX(const FShotCosmeticEvent& Event,
                     AActor* ConfirmedHitActor);
```

Reliable `ServerFire` is acceptable for a semi-automatic M2 weapon with bounded click rate. If automatic fire is added later, change to reliable start/stop intent or an Unreliable shot stream; never queue every automatic round reliably.

## Server validation order

1. Caller owns the component's Character and Character is alive.
2. Equipped weapon exists, is owned/equipped by this Character, and has valid WeaponData.
3. `ClientShotId` is newer than the last accepted/processed ID within wrap-safe comparison.
4. Server time satisfies `LastAcceptedFireTime + FireInterval` with a small configured epsilon.
5. Authoritative ammo is greater than zero.
6. Aim direction is normalized and within `MaxAimErrorDegrees` of current authoritative/presented aim.
7. Server derives muzzle origin from authoritative Character/weapon Transform and checks origin error internally.
8. Server decrements ammo, traces on the weapon trace channel, applies damage through `UHealthComponent`, then emits cosmetic event.

Rejection never decrements ammo or applies damage. Owner-predicted muzzle flash does not need to be undone; ammo UI is corrected to authoritative value.

## Collision contract

Add named project trace channel `WeaponTrace` in config/Editor:

- World static: Block.
- Enemy/test damage target capsule: Block.
- Player capsule: choose Ignore for M2 to disable friendly fire and self obstruction.
- Cosmetic sprite/mesh planes: Ignore.
- Interaction-only volumes: Ignore.

Server and client cosmetic traces must not use different channels. The Server trace is the only gameplay trace.

## Assets

- `DA_Weapon_TestRifle` with the values from `network-contracts.json`.
- One placeholder weapon Actor/mesh attached to a named socket or temporary scene component.
- `BP_TestDamageTarget` with `UHealthComponent` and a replicated dead-state visual.
- Input action `IA_Fire` and mapping.

## Acceptance criteria

- Host and client can each fire; all peers see accepted shot cosmetics.
- A client hit reduces Server-owned target health exactly once and all peers converge on health/death state.
- Fire-rate spam, zero-ammo fire, stale sequence, invalid aim, and dead-player fire are rejected.
- Client cannot choose damage, origin, hit Actor, or ammo.
- Late joining client sees current target death and player life state without replaying shot Multicasts.
- At 100 ms RTT the owner sees immediate muzzle feedback while gameplay result arrives from Server.
- At 5% packet loss, missed cosmetic events do not cause health/ammo divergence.
- Four-player PIE produces no ownership/RPC warnings.

## Deferred after M2

Reloading, automatic fire, shotgun pellets, projectiles, explosions, friendly-fire options, armor, hit zones, lag compensation, AI attacks, and generalized Gameplay Effects.

