#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Persistence/ProfileSaveTypes.h"
#include "ExtractionSaveGame.generated.h"

/** Versioned local profile. It deliberately contains no replicated actor or widget state. */
UCLASS()
class MYPROJ2_API UExtractionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 SaveVersion = 1;

	UPROPERTY(SaveGame)
	FGuid ProfileId;

	UPROPERTY(SaveGame)
	int64 SaveGeneration = 0;

	/** Persistent 5x20 warehouse. */
	UPROPERTY(SaveGame)
	FSavedInventory Stash;

	/** Base-side carried loadout. It is removed from Stash before entering a raid. */
	UPROPERTY(SaveGame)
	FPreparedRaidLoadout PreparedLoadout;

	/** Crash-safe handoff between pressing Enter Raid and the next map's PlayerController. */
	UPROPERTY(SaveGame)
	FPreparedRaidLoadout PendingRaidLoadout;

	UPROPERTY(SaveGame)
	int64 StashCurrency = 0;
};
