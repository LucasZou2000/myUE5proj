// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "MYPROJ2GameState.generated.h"

/**
 * Replicated raid-wide state. Server-only mutations go through GameMode.
 * Clients compute phase countdown from PhaseEndServerTime using GetServerWorldTimeSeconds().
 */
UCLASS()
class MYPROJ2_API AMYPROJ2GameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	AMYPROJ2GameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority-only phase transition. Called exclusively by GameMode on Server. */
	void AuthoritySetRaidPhase(ERaidPhase NewPhase, double NewPhaseEndServerTime);

	UFUNCTION(BlueprintPure, Category = "Raid")
	ERaidPhase GetRaidPhase() const { return RaidPhase; }

	UFUNCTION(BlueprintPure, Category = "Raid")
	double GetPhaseEndServerTime() const { return PhaseEndServerTime; }

protected:

	UFUNCTION()
	void OnRep_RaidPhase(ERaidPhase PreviousPhase);

	UPROPERTY(ReplicatedUsing = OnRep_RaidPhase, BlueprintReadOnly, Category = "Raid")
	ERaidPhase RaidPhase = ERaidPhase::Lobby;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	double PhaseEndServerTime = 0.0;
};
