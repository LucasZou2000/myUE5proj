// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "MYPROJ2GameMode.generated.h"

class AMYPROJ2GameState;
class AMYPROJ2PlayerState;

/**
 *  Simple Game Mode for a top-down perspective game
 *  Sets the default gameplay framework classes
 *  Check the Blueprint derived class for the set values
 *
 *  M0: Server-only raid phase orchestration + PostLogin/Logout logging.
 */
UCLASS(abstract)
class MYPROJ2_API AMYPROJ2GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	AMYPROJ2GameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * Authority-only raid phase transition. Updates GameState replicated state.
	 * GameMode is Server-only so callers are inherently authoritative.
	 */
	UFUNCTION(BlueprintCallable, Category = "Raid")
	void SetRaidPhase(ERaidPhase NewPhase, double NewPhaseEndServerTime);
};
