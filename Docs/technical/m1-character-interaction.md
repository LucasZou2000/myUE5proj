# M1 Character and Interaction

Depends on: M0 accepted and `network-architecture.md`.

## Objective

Replace click-to-move with WASD twin-stick movement, local mouse aiming, replicated facing for remote presentation, and one secure generic interaction path. No inventory or loot content is implemented.

## Required classes

```text
Source/MYPROJ2/Character/MYPROJ2CharacterBase.h/.cpp
Source/MYPROJ2/Interaction/Interactable.h
Source/MYPROJ2/Interaction/InteractionComponent.h/.cpp
Source/MYPROJ2/Interaction/InteractionTypes.h
Source/MYPROJ2/Interaction/TestInteractable.h/.cpp
```

If renaming the current `AMYPROJ2Character` would break Blueprint references, keep the existing class name and implement the same contract there. Do not create parallel production player classes with overlapping responsibility.

## Input and movement

- Enhanced Input actions: `IA_Move` (`Axis2D`), `IA_Aim` (`Axis2D` only if gamepad is supported now), `IA_Interact` (`Digital`). Mouse aim uses cursor deprojection/ground-plane hit locally.
- Movement calls `AddMovementInput`; it never calls `SetActorLocation` for normal locomotion.
- Character remains constrained to the gameplay plane and uses native CharacterMovement replication.
- Disable `bOrientRotationToMovement`. Owning player faces local aim yaw immediately.
- Server receives compressed aim yaw at no more than `AimSendRateHz`; simulated proxies interpolate presentation yaw.
- If setting Actor rotation interferes with CharacterMovement, keep authoritative capsule yaw controlled on Server and rotate a dedicated visual root locally/remotely. Document the chosen approach in `chat.md`.

## Types

```cpp
USTRUCT(BlueprintType)
struct FInteractionQueryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<AActor> Target = nullptr;

    UPROPERTY(BlueprintReadOnly)
    FText Prompt;

    UPROPERTY(BlueprintReadOnly)
    bool bCanInteract = false;
};

UENUM(BlueprintType)
enum class EInteractionRejectReason : uint8
{
    None,
    InvalidTarget,
    TooFar,
    Obstructed,
    Unavailable,
    Duplicate
};
```

`FInteractionQueryResult` is local-only UI state and is not replicated.

## Interface

```cpp
UINTERFACE(BlueprintType)
class UInteractable : public UInterface { GENERATED_BODY() };

class IInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FText GetInteractionPrompt(const APawn* Viewer) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool CanInteract(const APawn* InstigatorPawn) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void ExecuteInteraction(APawn* InstigatorPawn);
};
```

`ExecuteInteraction` is called on Authority only. Blueprint implementations must not issue a second Server RPC.

## Component contract

```cpp
UCLASS(ClassGroup=(Gameplay), meta=(BlueprintSpawnableComponent))
class UInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    void UpdateLocalFocus();
    void TryInteract();

protected:
    UFUNCTION(Server, Reliable)
    void ServerTryInteract(AActor* Target, uint16 ClientSequence);

    UFUNCTION(Client, Reliable)
    void ClientInteractionRejected(uint16 ClientSequence,
                                   EInteractionRejectReason Reason);
};
```

Local focus trace may run every frame because it is presentation-only. Server validation performs its own trace or at minimum verifies target, distance from authoritative Pawn location, unobstructed visibility, interface, and `CanInteract`.

Do not place `ServerTryInteract` on `ATestInteractable`; clients do not own world interactables.

## Test interactable

`ATestInteractable` owns one durable replicated boolean:

```cpp
UPROPERTY(ReplicatedUsing=OnRep_Activated)
bool bActivated = false;
```

Interaction toggles or activates it on Server. Color/material change happens in `OnRep_Activated` and also runs on Listen Server through a shared presentation method.

## Acceptance criteria

- Host and client both move with WASD and aim independently.
- Remote movement uses native smoothing and remains correct at 100 ms RTT.
- Remote facing is visually smooth and aim traffic is capped at 15 Hz.
- Both players can activate the test interactable and see the same durable state.
- Client cannot interact from outside the configured distance or through a blocking wall.
- Spamming interact does not execute the same sequence twice.
- No gameplay result is produced by the local focus trace alone.
- No M2 combat class, inventory type, or loot type is added.

## Stop conditions

Do not start M2 while movement corrections are frequent during ordinary WASD input, facing changes cause capsule collision instability, or interaction can be triggered on a client without Server confirmation.

