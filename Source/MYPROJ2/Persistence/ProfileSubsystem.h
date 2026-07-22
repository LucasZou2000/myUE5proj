#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Persistence/ProfileSaveTypes.h"
#include "ProfileSubsystem.generated.h"

class UExtractionSaveGame;
class UItemDefinition;
class UInventoryComponent;

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
	static constexpr int32 CurrentColumns = 10;
	static constexpr int32 CurrentRows = 6;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	EProfileLoadResult LoadOrCreateProfile();
	bool SaveProfile();

	const UExtractionSaveGame* GetProfile() const { return Profile; }
	UExtractionSaveGame* GetMutableProfile() { return Profile; }

	/** Atomic local base operations. They are intentionally UI-independent. */
	/** Directly transfers between the persistent stash and the current carried inventory. */
	bool MoveAllStashToInventory(UInventoryComponent* Inventory, int64& InOutCarriedCurrency);
	bool MoveAllInventoryToStash(UInventoryComponent* Inventory, int64& InOutCarriedCurrency);
	bool MoveItem(FSavedInventory& Source, FSavedInventory& Destination, const FGuid& ItemId, int32 Quantity);

	/** Persists/restores the current carried inventory around level travel. */
	bool SaveCurrentInventory(UInventoryComponent* Inventory, int64 CarriedCurrency);
	bool LoadCurrentInventory(UInventoryComponent* Inventory, int64& OutCarriedCurrency);
	bool ApplyRaidSettlement(const FRaidSettlementPayload& Settlement);

	/** Development-only seed for validating the currency flow before currency loot exists. */
	bool DebugGrantStashCurrency(int64 Amount);

	FString GetInventorySummary(const FSavedInventory& Inventory) const;
	FString GetInventorySummary(const UInventoryComponent* Inventory) const;

private:
	bool ValidateAndRepairInventory(FSavedInventory& Inventory, const TCHAR* Context);
	bool AddItemToInventory(FSavedInventory& Destination, const FItemInstance& SourceItem, int32 Quantity) const;
	bool MoveAll(FSavedInventory& Source, FSavedInventory& Destination);
	bool SnapshotInventory(const UInventoryComponent* Inventory, FSavedInventory& OutInventory) const;
	UItemDefinition* ResolveDefinition(const FPrimaryAssetId& DefinitionId) const;
	void EnsureDefaultLayout(UExtractionSaveGame& Save) const;

	UPROPERTY(Transient)
	TObjectPtr<UExtractionSaveGame> Profile;
};
