// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMYPROJ2Net, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMYPROJ2Interaction, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogMYPROJ2Combat, Log, All);

/**
 * High-level raid phase replicated from GameState.
 * Clients compute countdown from PhaseEndServerTime; no ticking seconds value is replicated.
 */
UENUM(BlueprintType)
enum class ERaidPhase : uint8
{
	Lobby,
	Deploying,
	InRaid,
	Resolving,
	Returning
};

/**
 * Durable per-player life state. M0 only uses Alive; reserved entries prevent later renaming churn.
 */
UENUM(BlueprintType)
enum class EPlayerLifeState : uint8
{
	Alive,
	DownedReserved,
	Dead,
	Extracted
};
