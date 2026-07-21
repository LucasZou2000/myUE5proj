// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MYPROJ2CharacterBase.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInteractionComponent;
class UCombatComponent;
class UHealthComponent;
class UDecalComponent;
class UNiagaraSystem;
struct FInputActionValue;
class UInputAction;

/**
 * Raid-era player pawn. WASD-driven, top-down camera, cursor-derived aim.
 *
 * Movement model (M1):
 *   - WASD driven via AddMovementInput. No click-to-move, no pathfinding.
 *   - bOrientRotationToMovement = false. Capsule yaw is decoupled from velocity.
 *   - Server computes AimYaw from the client's cursor position (sent via ServerUpdateAim)
 *     and uses it to rotate the capsule. Simulated proxies see capsule rotation
 *     through the standard replicated movement pipeline.
 *
 * Aim model (M1):
 *   - Owning client samples cursor world position every frame.
 *   - Local visual root (mesh) yaws instantly toward the cursor for responsiveness.
 *   - ServerUpdateAim is rate-limited to NetworkSettings::AimSendRateHz.
 *   - Server stores ServerAimYaw and uses it for capsule rotation + later M2 fire validation.
 *   - ServerAimYaw is replicated to simulated proxies; they slerp their visual root toward it.
 */
UCLASS(Blueprintable)
class MYPROJ2_API AMYPROJ2CharacterBase : public ACharacter
{
	GENERATED_BODY()

public:

	AMYPROJ2CharacterBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Returns the top-down camera. */
	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }

	/** Returns the camera boom. */
	UFUNCTION(BlueprintPure, Category = "Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns the interaction component. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	UInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** Returns the combat component (M2). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/** Returns the health component (M2). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Last aim yaw (degrees) applied to the capsule on the Server / replicated to proxies. */
	UFUNCTION(BlueprintPure, Category = "Aim")
	float GetServerAimYaw() const { return ServerAimYaw; }

protected:

	/** Bound to IA_Move (Axis2D). */
	void OnMoveInput(const FInputActionValue& Value);

	/** Bound to IA_Interact (Digital). */
	void OnInteractInput(const FInputActionValue& Value);

	/** Bound to IA_Fire (Digital). M2. */
	void OnFireInput(const FInputActionValue& Value);

	/** Owning client: compute world-space cursor location and drive local visual root. Returns false if cursor not over floor. */
	bool SampleCursorWorldLocation(FVector& OutWorldLocation) const;

	/** Owning client tick: local visual root rotation + rate-limited aim upload. */
	void TickLocalAim(float DeltaSeconds);

	/** Simulated proxy tick: smoothly interpolate visual root toward ServerAimYaw. */
	void TickSimulatedAim(float DeltaSeconds);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateAim(float NewAimYaw);

private:

	/** Top-down camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	/** Camera boom. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Visual root that yaws toward cursor locally and ServerAimYaw on proxies. Mesh is attached here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> VisualRoot;

	/** Interaction driver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInteractionComponent> InteractionComponent;

	/** Combat validation + ammo (M2). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatComponent> CombatComponent;

	/** Replicated health pool (M2). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> HealthComponent;

	/** Server-authoritative aim yaw (degrees). Replicated to simulated proxies. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aim", meta = (AllowPrivateAccess = "true"))
	float ServerAimYaw = 0.0f;

	/** Last aim yaw sent by the owning client. Used to avoid spamming identical values. */
	float LastSentAimYaw = 0.0f;

	/** Time accumulated since the last ServerUpdateAim send. */
	float TimeSinceLastAimSend = 0.0f;

	/** Cached cursor world location on owning client. */
	FVector LocalAimTarget = FVector::ZeroVector;

	/** True when the local cursor was over a valid floor surface last frame. */
	bool bLocalCursorValid = false;

public:

	/** Enhanced Input action: WASD movement (Axis2D). Bound in SetupPlayerInputComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Enhanced Input action: interact (Digital). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	/** Enhanced Input action: fire weapon (Digital). M2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	/** Enhanced Input action: toggle inventory UI (Digital). M3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	/** Bound to IA_Inventory (Digital). M3. */
	void OnInventoryInput(const FInputActionValue& Value);

	/** M3: create/show/hide the inventory UI widget. Only runs on the owning client. */
	void ToggleInventoryUI();

	/** Shows the existing inventory UI without toggling it closed. */
	void ShowInventoryUI();

private:

	/** M3: cached inventory widget instance. Created on first use. */
	UPROPERTY(Transient)
	TObjectPtr<class UUserWidget> InventoryWidgetInstance;
};
