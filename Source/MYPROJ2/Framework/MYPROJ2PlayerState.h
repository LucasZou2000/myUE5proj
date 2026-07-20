// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "MYPROJ2PlayerState.generated.h"

/**
 * Per-player replicated raid identity and life state. Survives Pawn replacement.
 * RaidPlayerId is assigned on Server and is not a platform account ID.
 */
UCLASS()
class MYPROJ2_API AMYPROJ2PlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	AMYPROJ2PlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Authority-only. Called by GameMode during PostLogin. */
	void AuthorityAssignRaidPlayerId(const FGuid& NewId);

	/** Authority-only. Called by GameMode/combat code on Server. */
	void AuthoritySetLifeState(EPlayerLifeState NewState);

	UFUNCTION(BlueprintPure, Category = "Raid")
	const FGuid& GetRaidPlayerId() const { return RaidPlayerId; }

	UFUNCTION(BlueprintPure, Category = "Raid")
	EPlayerLifeState GetLifeState() const { return LifeState; }

protected:

	UFUNCTION()
	void OnRep_LifeState(EPlayerLifeState PreviousState);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Raid")
	FGuid RaidPlayerId;

	UPROPERTY(ReplicatedUsing = OnRep_LifeState, BlueprintReadOnly, Category = "Raid")
	EPlayerLifeState LifeState = EPlayerLifeState::Alive;
};
