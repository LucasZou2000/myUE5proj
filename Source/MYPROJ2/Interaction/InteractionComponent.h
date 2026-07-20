// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionTypes.h"
#include "InteractionComponent.generated.h"

class ACharacter;

/**
 * Per-character interaction driver. Lives on the player pawn.
 *
 * Local (owning client):
 *   - Tick performs a local focus trace to drive UI prompt via UpdateLocalFocus().
 *   - TryInteract() is invoked by input and fires ServerTryInteract.
 *
 * Server:
 *   - ServerTryInteract performs authoritative validation (availability, distance, LOS)
 *     against the target's CURRENT server-side state, then dispatches IInteractable::ExecuteInteraction.
 *
 * Duplicate protection: a monotonically increasing InteractSequence is replicated so the
 * Server can drop replays of the same button press.
 */
UCLASS(ClassGroup = (MYPROJ2), meta = (BlueprintSpawnableComponent))
class MYPROJ2_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UInteractionComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Bound from input on the owning client. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/** Current local focus state for UI. Local-only. */
	const FInteractionQueryResult& GetCurrentFocus() const { return CurrentFocus; }

	/** Cached copy of the local focus for UI binding. Updated in TickComponent on owning client. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FInteractionQueryResult CurrentFocus;

protected:

	/** Recompute CurrentFocus. Owning client only. */
	void UpdateLocalFocus();

	/** Find the best interactable actor under the cursor within MaxFocusDistance. */
	AActor* FindFocusTarget(FHitResult& OutHit) const;

	UFUNCTION(Server, Reliable)
	void ServerTryInteract(AActor* Target, int32 Sequence);

	UFUNCTION(Client, Reliable)
	void ClientInteractionRejected(AActor* Target, EInteractionRejectReason Reason);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Interaction")
	int32 InteractSequence = 0;

private:

	/** Last sequence the Server has consumed. Used to drop replays. */
	int32 LastConsumedSequence = 0;
};
