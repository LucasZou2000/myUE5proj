#include "Persistence/ProfileSubsystem.h"

#include "Engine/AssetManager.h"
#include "Inventory/InventoryGridLibrary.h"
#include "Inventory/InventoryComponent.h"
#include "Items/ItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Persistence/ExtractionSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogMYPROJ2Profile, Log, All);

namespace
{
	const FString PrimarySlot(TEXT("MYPROJ2_Profile_0"));
	const FString BackupSlot(TEXT("MYPROJ2_Profile_0_Backup"));
	constexpr int32 LocalUserIndex = 0;

	FIntPoint GetFootprint(const FItemInstance& Item, UItemDefinition* Definition)
	{
		return Definition ? Definition->GetEffectiveGridSize(Item.bRotated) : FIntPoint::ZeroValue;
	}
}

void UProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadOrCreateProfile();
}

EProfileLoadResult UProfileSubsystem::LoadOrCreateProfile()
{
	auto TryLoad = [this](const FString& Slot) -> UExtractionSaveGame*
	{
		return Cast<UExtractionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, LocalUserIndex));
	};

	UExtractionSaveGame* Loaded = nullptr;
	EProfileLoadResult Result = EProfileLoadResult::Failed;
	if (UGameplayStatics::DoesSaveGameExist(PrimarySlot, LocalUserIndex))
	{
		Loaded = TryLoad(PrimarySlot);
		Result = EProfileLoadResult::Loaded;
	}
	if (!Loaded && UGameplayStatics::DoesSaveGameExist(BackupSlot, LocalUserIndex))
	{
		Loaded = TryLoad(BackupSlot);
		Result = EProfileLoadResult::RecoveredBackup;
	}
	if (!Loaded)
	{
		if (UGameplayStatics::DoesSaveGameExist(PrimarySlot, LocalUserIndex) ||
			UGameplayStatics::DoesSaveGameExist(BackupSlot, LocalUserIndex))
		{
			UE_LOG(LogMYPROJ2Profile, Error, TEXT("Profile load failed for both primary and backup slots."));
			return EProfileLoadResult::Failed;
		}
		Loaded = Cast<UExtractionSaveGame>(UGameplayStatics::CreateSaveGameObject(UExtractionSaveGame::StaticClass()));
		if (!Loaded)
		{
			return EProfileLoadResult::Failed;
		}
		Loaded->ProfileId = FGuid::NewGuid();
		Loaded->SaveVersion = CurrentSaveVersion;
		Result = EProfileLoadResult::CreatedNew;
	}
	if (Loaded->SaveVersion > CurrentSaveVersion)
	{
		UE_LOG(LogMYPROJ2Profile, Error, TEXT("Profile version %d is newer than supported version %d."),
			Loaded->SaveVersion, CurrentSaveVersion);
		return EProfileLoadResult::Failed;
	}
	if (Loaded->SaveVersion < CurrentSaveVersion)
	{
		// Version 2 adds persisted current carried state. Existing stash data remains untouched.
		Loaded->SaveVersion = CurrentSaveVersion;
	}
	EnsureDefaultLayout(*Loaded);
	ValidateAndRepairInventory(Loaded->Stash, TEXT("Stash"));
	ValidateAndRepairInventory(Loaded->CurrentInventory, TEXT("CurrentInventory"));
	Profile = Loaded;

	if (Result == EProfileLoadResult::CreatedNew)
	{
		SaveProfile();
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Profile %s: id=%s generation=%lld"),
		Result == EProfileLoadResult::Loaded ? TEXT("loaded") :
		(Result == EProfileLoadResult::CreatedNew ? TEXT("created") : TEXT("recovered backup")),
		*Profile->ProfileId.ToString(), Profile->SaveGeneration);
	return Result;
}

bool UProfileSubsystem::SaveProfile()
{
	if (!Profile)
	{
		return false;
	}
	if (UGameplayStatics::DoesSaveGameExist(PrimarySlot, LocalUserIndex))
	{
		USaveGame* Previous = UGameplayStatics::LoadGameFromSlot(PrimarySlot, LocalUserIndex);
		if (!Previous || !UGameplayStatics::SaveGameToSlot(Previous, BackupSlot, LocalUserIndex))
		{
			UE_LOG(LogMYPROJ2Profile, Warning, TEXT("Failed to refresh profile backup; primary was not overwritten."));
			return false;
		}
	}
	const int64 PreviousGeneration = Profile->SaveGeneration;
	++Profile->SaveGeneration;
	if (!UGameplayStatics::SaveGameToSlot(Profile, PrimarySlot, LocalUserIndex))
	{
		Profile->SaveGeneration = PreviousGeneration;
		UE_LOG(LogMYPROJ2Profile, Error, TEXT("Failed to write profile primary slot."));
		return false;
	}
	return true;
}

void UProfileSubsystem::EnsureDefaultLayout(UExtractionSaveGame& Save) const
{
	Save.Stash.GridSize = FIntPoint(StashColumns, StashRows);
	if (Save.CurrentInventory.GridSize.X <= 0 || Save.CurrentInventory.GridSize.Y <= 0)
	{
		Save.CurrentInventory.GridSize = FIntPoint(CurrentColumns, CurrentRows);
	}
}

UItemDefinition* UProfileSubsystem::ResolveDefinition(const FPrimaryAssetId& DefinitionId) const
{
	if (!DefinitionId.IsValid())
	{
		return nullptr;
	}
	UAssetManager& AssetManager = UAssetManager::Get();
	if (UItemDefinition* Loaded = Cast<UItemDefinition>(AssetManager.GetPrimaryAssetObject(DefinitionId)))
	{
		return Loaded;
	}
	return Cast<UItemDefinition>(AssetManager.GetPrimaryAssetPath(DefinitionId).TryLoad());
}

bool UProfileSubsystem::ValidateAndRepairInventory(FSavedInventory& Inventory, const TCHAR* Context)
{
	if (Inventory.GridSize.X <= 0 || Inventory.GridSize.Y <= 0)
	{
		UE_LOG(LogMYPROJ2Profile, Warning, TEXT("%s had invalid grid dimensions; clearing entries."), Context);
		Inventory.Items.Reset();
		return false;
	}
	TArray<FItemInstance> ValidItems;
	TSet<FGuid> SeenIds;
	bool bAllValid = true;
	for (const FItemInstance& Item : Inventory.Items)
	{
		UItemDefinition* Definition = ResolveDefinition(Item.DefinitionId);
		const FIntPoint Footprint = GetFootprint(Item, Definition);
		TFunction<const FItemInstance&(const FItemInstance&)> GetItem =
			[](const FItemInstance& Value) -> const FItemInstance& { return Value; };
		TFunction<FIntPoint(const FItemInstance&)> GetSize = [this](const FItemInstance& Value)
		{
			if (UItemDefinition* ValueDefinition = ResolveDefinition(Value.DefinitionId))
			{
				return ValueDefinition->GetEffectiveGridSize(Value.bRotated);
			}
			return FIntPoint::ZeroValue;
		};
		const bool bValid = Item.IsValid() && Definition && Item.Quantity <= FMath::Max(1, Definition->MaxStack) &&
			FInventoryGridLibrary::IsInBounds(Inventory.GridSize, Item.GridPosition, Footprint) &&
			!SeenIds.Contains(Item.InstanceId) &&
			!FInventoryGridLibrary::WouldOverlap(ValidItems, GetItem, GetSize, Item.GridPosition, Footprint, FGuid());
		if (!bValid)
		{
			bAllValid = false;
			UE_LOG(LogMYPROJ2Profile, Warning, TEXT("Dropped invalid saved item %s from %s."),
				*Item.DefinitionId.ToString(), Context);
			continue;
		}
		SeenIds.Add(Item.InstanceId);
		ValidItems.Add(Item);
	}
	Inventory.Items = MoveTemp(ValidItems);
	return bAllValid;
}

bool UProfileSubsystem::AddItemToInventory(FSavedInventory& Destination, const FItemInstance& SourceItem, int32 Quantity) const
{
	UItemDefinition* Definition = ResolveDefinition(SourceItem.DefinitionId);
	if (!Definition || Quantity <= 0)
	{
		return false;
	}
	TArray<FItemInstance> Staged = Destination.Items;
	int32 Remaining = Quantity;
	const int32 MaxStack = FMath::Max(1, Definition->MaxStack);
	for (FItemInstance& Entry : Staged)
	{
		if (Entry.DefinitionId == SourceItem.DefinitionId && Entry.Quantity < MaxStack)
		{
			const int32 Added = FMath::Min(MaxStack - Entry.Quantity, Remaining);
			Entry.Quantity += Added;
			Remaining -= Added;
			if (Remaining == 0)
			{
				Destination.Items = MoveTemp(Staged);
				return true;
			}
		}
	}
	TFunction<const FItemInstance&(const FItemInstance&)> GetItem =
		[](const FItemInstance& Value) -> const FItemInstance& { return Value; };
	TFunction<FIntPoint(const FItemInstance&)> GetSize = [this](const FItemInstance& Value)
	{
		if (UItemDefinition* ValueDefinition = ResolveDefinition(Value.DefinitionId))
		{
			return ValueDefinition->GetEffectiveGridSize(Value.bRotated);
		}
		return FIntPoint::ZeroValue;
	};
	bool bUseSourceId = !Staged.ContainsByPredicate([&SourceItem](const FItemInstance& Value)
	{
		return Value.InstanceId == SourceItem.InstanceId;
	});
	const FIntPoint Footprint = Definition->GetEffectiveGridSize(SourceItem.bRotated);
	while (Remaining > 0)
	{
		FIntPoint Position;
		if (!FInventoryGridLibrary::FindFirstFit(Destination.GridSize, Staged, GetItem, GetSize, Footprint, Position))
		{
			return false;
		}
		FItemInstance NewItem = SourceItem;
		NewItem.InstanceId = bUseSourceId ? SourceItem.InstanceId : FGuid::NewGuid();
		NewItem.Quantity = FMath::Min(Remaining, MaxStack);
		NewItem.GridPosition = Position;
		Staged.Add(NewItem);
		bUseSourceId = false;
		Remaining -= NewItem.Quantity;
	}
	Destination.Items = MoveTemp(Staged);
	return true;
}

bool UProfileSubsystem::MoveItem(FSavedInventory& Source, FSavedInventory& Destination, const FGuid& ItemId, int32 Quantity)
{
	const FItemInstance* SourceItem = Source.Items.FindByPredicate([ItemId](const FItemInstance& Item) { return Item.InstanceId == ItemId; });
	if (!SourceItem || Quantity <= 0 || Quantity > SourceItem->Quantity)
	{
		return false;
	}
	FSavedInventory StagedSource = Source;
	FSavedInventory StagedDestination = Destination;
	if (!AddItemToInventory(StagedDestination, *SourceItem, Quantity))
	{
		return false;
	}
	FItemInstance* MutableSource = StagedSource.Items.FindByPredicate([ItemId](const FItemInstance& Item) { return Item.InstanceId == ItemId; });
	MutableSource->Quantity -= Quantity;
	if (MutableSource->Quantity == 0)
	{
		StagedSource.Items.RemoveAll([ItemId](const FItemInstance& Item) { return Item.InstanceId == ItemId; });
	}
	Source = MoveTemp(StagedSource);
	Destination = MoveTemp(StagedDestination);
	return true;
}

bool UProfileSubsystem::MoveAll(FSavedInventory& Source, FSavedInventory& Destination)
{
	FSavedInventory StagedSource = Source;
	FSavedInventory StagedDestination = Destination;
	for (const FItemInstance& Item : Source.Items)
	{
		if (!AddItemToInventory(StagedDestination, Item, Item.Quantity))
		{
			return false;
		}
	}
	StagedSource.Items.Reset();
	Source = MoveTemp(StagedSource);
	Destination = MoveTemp(StagedDestination);
	return true;
}

bool UProfileSubsystem::SnapshotInventory(const UInventoryComponent* Inventory, FSavedInventory& OutInventory) const
{
	if (!Inventory)
	{
		return false;
	}
	OutInventory.GridSize = Inventory->GetGridSize();
	OutInventory.Items.Reset();
	for (const FReplicatedInventoryEntry& Entry : Inventory->GetEntries())
	{
		OutInventory.Items.Add(Entry.Item);
	}
	return true;
}

bool UProfileSubsystem::MoveAllStashToInventory(UInventoryComponent* Inventory, int64& InOutCarriedCurrency)
{
	if (!Profile || !Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority())
	{
		return false;
	}
	FSavedInventory PreviousStash = Profile->Stash;
	FSavedInventory PreviousInventory;
	if (!SnapshotInventory(Inventory, PreviousInventory))
	{
		return false;
	}
	FSavedInventory StagedStash = Profile->Stash;
	FSavedInventory StagedInventory = PreviousInventory;
	if (!MoveAll(StagedStash, StagedInventory))
	{
		UE_LOG(LogMYPROJ2Profile, Warning, TEXT("Take all failed: current inventory has insufficient space."));
		return false;
	}
	EInventoryRejectReason Reason = EInventoryRejectReason::None;
	if (!Inventory->AuthorityReplaceAll(StagedInventory.Items, Reason))
	{
		return false;
	}
	const int64 PreviousStashCurrency = Profile->StashCurrency;
	const int64 PreviousCarriedCurrency = InOutCarriedCurrency;
	if (PreviousStashCurrency > MAX_int64 - InOutCarriedCurrency)
	{
		Inventory->AuthorityReplaceAll(PreviousInventory.Items, Reason);
		return false;
	}
	Profile->Stash = MoveTemp(StagedStash);
	Profile->CurrentInventory = StagedInventory;
	Profile->CurrentInventory.GridSize = Inventory->GetGridSize();
	InOutCarriedCurrency += Profile->StashCurrency;
	Profile->CurrentCurrency = InOutCarriedCurrency;
	Profile->StashCurrency = 0;
	if (!SaveProfile())
	{
		Profile->Stash = MoveTemp(PreviousStash);
		Profile->CurrentInventory = PreviousInventory;
		Profile->StashCurrency = PreviousStashCurrency;
		Profile->CurrentCurrency = PreviousCarriedCurrency;
		InOutCarriedCurrency = PreviousCarriedCurrency;
		Inventory->AuthorityReplaceAll(PreviousInventory.Items, Reason);
		return false;
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Take all: stash -> current inventory, currency=%lld."), InOutCarriedCurrency - PreviousCarriedCurrency);
	return true;
}

bool UProfileSubsystem::MoveAllInventoryToStash(UInventoryComponent* Inventory, int64& InOutCarriedCurrency)
{
	if (!Profile || !Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority())
	{
		return false;
	}
	FSavedInventory PreviousStash = Profile->Stash;
	FSavedInventory PreviousInventory;
	if (!SnapshotInventory(Inventory, PreviousInventory))
	{
		return false;
	}
	FSavedInventory StagedStash = Profile->Stash;
	FSavedInventory StagedInventory = PreviousInventory;
	if (!MoveAll(StagedInventory, StagedStash))
	{
		UE_LOG(LogMYPROJ2Profile, Warning, TEXT("Store all failed: stash has insufficient space."));
		return false;
	}
	const int64 PreviousStashCurrency = Profile->StashCurrency;
	const int64 PreviousCarriedCurrency = InOutCarriedCurrency;
	if (InOutCarriedCurrency < 0 || InOutCarriedCurrency > MAX_int64 - Profile->StashCurrency)
	{
		return false;
	}
	Profile->Stash = MoveTemp(StagedStash);
	Profile->CurrentInventory = FSavedInventory();
	Profile->CurrentInventory.GridSize = Inventory->GetGridSize();
	Profile->StashCurrency += InOutCarriedCurrency;
	Profile->CurrentCurrency = 0;
	InOutCarriedCurrency = 0;
	Inventory->AuthorityClearAll();
	if (!SaveProfile())
	{
		Profile->Stash = MoveTemp(PreviousStash);
		Profile->CurrentInventory = PreviousInventory;
		Profile->StashCurrency = PreviousStashCurrency;
		Profile->CurrentCurrency = PreviousCarriedCurrency;
		InOutCarriedCurrency = PreviousCarriedCurrency;
		EInventoryRejectReason Reason = EInventoryRejectReason::None;
		Inventory->AuthorityReplaceAll(PreviousInventory.Items, Reason);
		return false;
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Store all: current inventory -> stash."));
	return true;
}

bool UProfileSubsystem::SaveCurrentInventory(UInventoryComponent* Inventory, int64 CarriedCurrency)
{
	if (!Profile || !Inventory || CarriedCurrency < 0 || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority())
	{
		return false;
	}
	FSavedInventory CurrentInventory;
	if (!SnapshotInventory(Inventory, CurrentInventory))
	{
		return false;
	}
	const FSavedInventory PreviousInventory = Profile->CurrentInventory;
	const int64 PreviousCurrency = Profile->CurrentCurrency;
	Profile->CurrentInventory = MoveTemp(CurrentInventory);
	Profile->CurrentInventory.GridSize = FIntPoint(CurrentColumns, CurrentRows);
	Profile->CurrentCurrency = CarriedCurrency;
	if (!SaveProfile())
	{
		Profile->CurrentInventory = PreviousInventory;
		Profile->CurrentCurrency = PreviousCurrency;
		return false;
	}
	return true;
}

bool UProfileSubsystem::LoadCurrentInventory(UInventoryComponent* Inventory, int64& OutCarriedCurrency)
{
	if (!Profile || !Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority())
	{
		return false;
	}
	EInventoryRejectReason Reason = EInventoryRejectReason::None;
	if (!Inventory->AuthorityReplaceAll(Profile->CurrentInventory.Items, Reason))
	{
		return false;
	}
	OutCarriedCurrency = Profile->CurrentCurrency;
	return true;
}

bool UProfileSubsystem::ApplyRaidSettlement(const FRaidSettlementPayload& Settlement)
{
	if (!Profile)
	{
		return false;
	}
	if (!Settlement.bExtracted)
	{
		const FSavedInventory PreviousCurrentInventory = Profile->CurrentInventory;
		const int64 PreviousCurrentCurrency = Profile->CurrentCurrency;
		Profile->CurrentInventory.Items.Reset();
		Profile->CurrentCurrency = 0;
		UE_LOG(LogMYPROJ2Profile, Log, TEXT("Raid death settlement: carried items and currency discarded."));
		if (!SaveProfile())
		{
			Profile->CurrentInventory = PreviousCurrentInventory;
			Profile->CurrentCurrency = PreviousCurrentCurrency;
			return false;
		}
		return true;
	}
	FSavedInventory StagedStash = Profile->Stash;
	for (const FItemInstance& Item : Settlement.Items)
	{
		if (!AddItemToInventory(StagedStash, Item, Item.Quantity))
		{
			UE_LOG(LogMYPROJ2Profile, Error, TEXT("Extraction settlement rejected: stash lacks space for %s."), *Item.DefinitionId.ToString());
			return false;
		}
	}
	if (Settlement.CarriedCurrency < 0 || Settlement.CarriedCurrency > MAX_int64 - Profile->StashCurrency)
	{
		return false;
	}
	const FSavedInventory PreviousStash = Profile->Stash;
	const int64 PreviousCurrency = Profile->StashCurrency;
	const FSavedInventory PreviousCurrentInventory = Profile->CurrentInventory;
	const int64 PreviousCurrentCurrency = Profile->CurrentCurrency;
	Profile->Stash = MoveTemp(StagedStash);
	Profile->StashCurrency += Settlement.CarriedCurrency;
	Profile->CurrentInventory.Items.Reset();
	Profile->CurrentCurrency = 0;
	const bool bSaved = SaveProfile();
	if (!bSaved)
	{
		Profile->Stash = PreviousStash;
		Profile->StashCurrency = PreviousCurrency;
		Profile->CurrentInventory = PreviousCurrentInventory;
		Profile->CurrentCurrency = PreviousCurrentCurrency;
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Extraction settlement: %d item stacks, currency=%lld, %s"),
		Settlement.Items.Num(), Settlement.CarriedCurrency, bSaved ? TEXT("saved") : TEXT("save failed"));
	return bSaved;
}

bool UProfileSubsystem::DebugGrantStashCurrency(int64 Amount)
{
	if (!Profile || Amount <= 0 || Amount > MAX_int64 - Profile->StashCurrency)
	{
		return false;
	}
	const int64 PreviousCurrency = Profile->StashCurrency;
	Profile->StashCurrency += Amount;
	if (!SaveProfile())
	{
		Profile->StashCurrency = PreviousCurrency;
		return false;
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Debug granted %lld stash currency."), Amount);
	return true;
}

FString UProfileSubsystem::GetInventorySummary(const FSavedInventory& Inventory) const
{
	FString Result = FString::Printf(TEXT("%dx%d, %d stacks"), Inventory.GridSize.X, Inventory.GridSize.Y, Inventory.Items.Num());
	for (const FItemInstance& Item : Inventory.Items)
	{
		const UItemDefinition* Definition = ResolveDefinition(Item.DefinitionId);
		const FString Name = Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName.ToString() : Item.DefinitionId.ToString();
		Result += FString::Printf(TEXT("\n%s x%d at (%d,%d)"), *Name, Item.Quantity, Item.GridPosition.X, Item.GridPosition.Y);
	}
	return Result;
}

FString UProfileSubsystem::GetInventorySummary(const UInventoryComponent* Inventory) const
{
	FSavedInventory Snapshot;
	return SnapshotInventory(Inventory, Snapshot) ? GetInventorySummary(Snapshot) : TEXT("Unavailable");
}
