#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Persistence/ProfileSaveTypes.h"
#include "ProfileSubsystem.generated.h"

class UExtractionSaveGame;
class UItemDefinition;

UENUM(BlueprintType)
enum class EProfileLoadResult : uint8
{
	Loaded,
	CreatedNew,
	RecoveredBackup,
	Failed
};

/** Owns the one local profile and is the only code path that reads/writes SaveGame slots. */
UCLASS()
class MYPROJ2_API UProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 2;
	static constexpr int32 StashColumns = 5;
	static constexpr int32 StashRows = 20;
	static constexpr int32 LoadoutColumns = 10;
	static constexpr int32 LoadoutRows = 6;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	EProfileLoadResult LoadOrCreateProfile();
	bool SaveProfile();

	const UExtractionSaveGame* GetProfile() const { return Profile; }
	UExtractionSaveGame* GetMutableProfile() { return Profile; }

	/** Atomic local base operations. They are intentionally UI-independent. */
	bool MoveAllStashToLoadout();
	bool MoveAllLoadoutToStash();
	bool MoveItem(FSavedInventory& Source, FSavedInventory& Destination, const FGuid& ItemId, int32 Quantity);
	bool MoveCurrencyToLoadout(int64 Amount);
	bool MoveCurrencyToStash(int64 Amount);

	/** Moves prepared items out of the warehouse domain before level travel. */
	bool BeginRaidFromPreparation();
	bool ConsumePendingRaidLoadout(FPreparedRaidLoadout& OutLoadout);
	bool ApplyRaidSettlement(const FRaidSettlementPayload& Settlement);

	/** Development-only seed for validating the currency flow before currency loot exists. */
	bool DebugGrantStashCurrency(int64 Amount);

	FString GetInventorySummary(const FSavedInventory& Inventory) const;

private:
	bool ValidateAndRepairInventory(FSavedInventory& Inventory, const TCHAR* Context);
	bool AddItemToInventory(FSavedInventory& Destination, const FItemInstance& SourceItem, int32 Quantity) const;
	bool MoveAll(FSavedInventory& Source, FSavedInventory& Destination);
	UItemDefinition* ResolveDefinition(const FPrimaryAssetId& DefinitionId) const;
	void EnsureDefaultLayout(UExtractionSaveGame& Save) const;

	UPROPERTY(Transient)
	TObjectPtr<UExtractionSaveGame> Profile;
};
