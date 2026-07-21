// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventoryTypes.h"
#include "MYPROJ2PlayerController.generated.h"

class UInputMappingContext;
class UInventoryComponent;
class ALootContainer;

/**
 * Top-down player controller (M1).
 *
 * WASD movement and cursor-derived aim are handled by the possessed Character
 * (see AMYPROJ2CharacterBase). This controller only manages the Enhanced Input
 * Mapping Context and cursor visibility. Click-to-move has been removed.
 *
 * M3: owns the player raid inventory (`UInventoryComponent`). A
 * PlayerController exists only on the Server and its owning client, so the
 * replicated Fast Array is naturally private without per-connection filters.
 * Pawn replacement does not destroy carried inventory.
 */
UCLASS(abstract)
class MYPROJ2_API AMYPROJ2PlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Context to apply on the local player. */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** True if the controlled pawn should show a cursor decal under the mouse. */
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bShowCursorDecal = true;

	/** BeginPlay override */
	virtual void BeginPlay() override;

	/** Setup input bindings */
	virtual void SetupInputComponent() override;

public:

	/** Constructor */
	AMYPROJ2PlayerController();

	/** Player raid inventory (M3). Server-authoritative; owner-only Fast Array. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** M4: client intent is routed through the owned controller, never a world container. */
	UFUNCTION(Server, Reliable)
	void ServerTakeFromContainer(ALootContainer* Container, FGuid ItemId, int32 Quantity, uint16 RequestId);

	UFUNCTION(Server, Reliable)
	void ServerPutIntoContainer(ALootContainer* Container, FGuid ItemId, int32 Quantity,
		FIntPoint TargetPosition, bool bRotated, uint16 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientOpenLootContainer(ALootContainer* Container);

	UFUNCTION(Client, Reliable)
	void ClientLootTransferRejected(uint16 RequestId, EInventoryRejectReason Reason);

private:

	/** Player raid inventory (M3). Created in the constructor so it exists on
	 *  both Server and owning client before BeginPlay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInventoryComponent> InventoryComponent;

	bool IsLootRequestStale(uint16 RequestId) const;
	bool ValidateLootContainerRequest(ALootContainer* Container, EInventoryRejectReason& OutReason) const;
	void MarkLootRequestConsumed(uint16 RequestId);

	uint16 LastConsumedLootRequestId = 0;
	bool bHasConsumedLootRequest = false;
};
