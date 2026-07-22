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

	/** Current carried inventory persisted before level travel and restored after travel. */
	UPROPERTY(SaveGame)
	FSavedInventory CurrentInventory;

	UPROPERTY(SaveGame)
	int64 CurrentCurrency = 0;

	UPROPERTY(SaveGame)
	int64 StashCurrency = 0;
};
