// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractionTypes.generated.h"

/**
 * Reason the Server rejected an interaction request. Sent to the owning client
 * via ClientInteractionRejected so the local UI can flash a meaningful message.
 */
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

/**
 * Local-only UI state produced by UInteractionComponent::UpdateLocalFocus.
 * Never replicated; Server performs its own authoritative validation.
 */
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
