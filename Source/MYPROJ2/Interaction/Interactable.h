// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionTypes.h"
#include "Interactable.generated.h"

class ACharacter;
class UInteractionComponent;

/**
 * Contract for actors that can be interacted with. M1 exposes the minimum:
 * availability check, local prompt text, and server-side execution entry.
 *
 * Server calls ExecuteInteraction only after authority-side validation
 * (availability, distance, LOS) has passed.
 */
UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class IInteractable
{
	GENERATED_BODY()

public:

	/**
	 * Authoritative availability check. Must be safe to call on Server or local UI.
	 * Pure virtual C++-only: UHT disallows BlueprintNativeEvent on a
	 * CannotImplementInterfaceInBlueprint interface, and M1 does not need BP
	 * implementations — ATestInteractable is C++.
	 */
	virtual bool IsAvailableForInteraction(const ACharacter* RequestingCharacter) const = 0;

	/** Local UI prompt. Never used by Server for validation. */
	virtual FText GetInteractionPrompt(const ACharacter* RequestingCharacter) const = 0;

	/**
	 * Server-side interaction entry. Called exactly once per accepted request.
	 * Implementations own any replicated state mutation.
	 */
	virtual void ExecuteInteraction(ACharacter* RequestingCharacter, UInteractionComponent* RequestingComponent) = 0;
};
