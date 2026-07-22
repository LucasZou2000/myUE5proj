#include "Persistence/ProfileSubsystem.h"

#include "Engine/AssetManager.h"
#include "Inventory/InventoryGridLibrary.h"
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
		// Version 2 adds explicit prepared/pending raid state. Existing stash data remains untouched.
		Loaded->SaveVersion = CurrentSaveVersion;
	}
	EnsureDefaultLayout(*Loaded);
	ValidateAndRepairInventory(Loaded->Stash, TEXT("Stash"));
	ValidateAndRepairInventory(Loaded->PreparedLoadout.Inventory, TEXT("PreparedLoadout"));
	ValidateAndRepairInventory(Loaded->PendingRaidLoadout.Inventory, TEXT("PendingRaidLoadout"));
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
	if (Save.PreparedLoadout.Inventory.GridSize.X <= 0 || Save.PreparedLoadout.Inventory.GridSize.Y <= 0)
	{
		Save.PreparedLoadout.Inventory.GridSize = FIntPoint(LoadoutColumns, LoadoutRows);
	}
	if (Save.PendingRaidLoadout.Inventory.GridSize.X <= 0 || Save.PendingRaidLoadout.Inventory.GridSize.Y <= 0)
	{
		Save.PendingRaidLoadout.Inventory.GridSize = FIntPoint(LoadoutColumns, LoadoutRows);
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

bool UProfileSubsystem::MoveAllStashToLoadout()
{
	if (!Profile)
	{
		return false;
	}
	const FSavedInventory PreviousStash = Profile->Stash;
	const FPreparedRaidLoadout PreviousLoadout = Profile->PreparedLoadout;
	const int64 PreviousCurrency = Profile->StashCurrency;
	if (!MoveAll(Profile->Stash, Profile->PreparedLoadout.Inventory))
	{
		UE_LOG(LogMYPROJ2Profile, Warning, TEXT("Withdraw all failed: loadout has insufficient space."));
		return false;
	}
	const int64 Currency = Profile->StashCurrency;
	if (Currency > MAX_int64 - Profile->PreparedLoadout.Currency)
	{
		Profile->Stash = PreviousStash;
		Profile->PreparedLoadout = PreviousLoadout;
		return false;
	}
	Profile->StashCurrency = 0;
	Profile->PreparedLoadout.Currency += Currency;
	const bool bSaved = SaveProfile();
	if (!bSaved)
	{
		Profile->Stash = PreviousStash;
		Profile->PreparedLoadout = PreviousLoadout;
		Profile->StashCurrency = PreviousCurrency;
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Withdraw all: %s"), bSaved ? TEXT("saved") : TEXT("save failed"));
	return bSaved;
}

bool UProfileSubsystem::MoveAllLoadoutToStash()
{
	if (!Profile)
	{
		return false;
	}
	const FSavedInventory PreviousStash = Profile->Stash;
	const FPreparedRaidLoadout PreviousLoadout = Profile->PreparedLoadout;
	const int64 PreviousCurrency = Profile->StashCurrency;
	if (!MoveAll(Profile->PreparedLoadout.Inventory, Profile->Stash))
	{
		UE_LOG(LogMYPROJ2Profile, Warning, TEXT("Deposit all failed: stash has insufficient space."));
		return false;
	}
	if (Profile->PreparedLoadout.Currency > MAX_int64 - Profile->StashCurrency)
	{
		Profile->Stash = PreviousStash;
		Profile->PreparedLoadout = PreviousLoadout;
		return false;
	}
	Profile->StashCurrency += Profile->PreparedLoadout.Currency;
	Profile->PreparedLoadout.Currency = 0;
	const bool bSaved = SaveProfile();
	if (!bSaved)
	{
		Profile->Stash = PreviousStash;
		Profile->PreparedLoadout = PreviousLoadout;
		Profile->StashCurrency = PreviousCurrency;
	}
	UE_LOG(LogMYPROJ2Profile, Log, TEXT("Deposit all: %s"), bSaved ? TEXT("saved") : TEXT("save failed"));
	return bSaved;
}

bool UProfileSubsystem::MoveCurrencyToLoadout(int64 Amount)
{
	if (!Profile || Amount <= 0 || Amount > Profile->StashCurrency || Amount > MAX_int64 - Profile->PreparedLoadout.Currency)
	{
		return false;
	}
	const int64 PreviousStashCurrency = Profile->StashCurrency;
	const int64 PreviousLoadoutCurrency = Profile->PreparedLoadout.Currency;
	Profile->StashCurrency -= Amount;
	Profile->PreparedLoadout.Currency += Amount;
	if (!SaveProfile())
	{
		Profile->StashCurrency = PreviousStashCurrency;
		Profile->PreparedLoadout.Currency = PreviousLoadoutCurrency;
		return false;
	}
	return true;
}

bool UProfileSubsystem::MoveCurrencyToStash(int64 Amount)
{
	if (!Profile || Amount <= 0 || Amount > Profile->PreparedLoadout.Currency || Amount > MAX_int64 - Profile->StashCurrency)
	{
		return false;
	}
	const int64 PreviousStashCurrency = Profile->StashCurrency;
	const int64 PreviousLoadoutCurrency = Profile->PreparedLoadout.Currency;
	Profile->PreparedLoadout.Currency -= Amount;
	Profile->StashCurrency += Amount;
	if (!SaveProfile())
	{
		Profile->StashCurrency = PreviousStashCurrency;
		Profile->PreparedLoadout.Currency = PreviousLoadoutCurrency;
		return false;
	}
	return true;
}

bool UProfileSubsystem::BeginRaidFromPreparation()
{
	if (!Profile || !Profile->PendingRaidLoadout.Inventory.Items.IsEmpty() || Profile->PendingRaidLoadout.Currency != 0)
	{
		return false;
	}
	const FPreparedRaidLoadout PreviousPrepared = Profile->PreparedLoadout;
	const FPreparedRaidLoadout PreviousPending = Profile->PendingRaidLoadout;
	Profile->PendingRaidLoadout = Profile->PreparedLoadout;
	Profile->PendingRaidLoadout.Inventory.GridSize = FIntPoint(LoadoutColumns, LoadoutRows);
	Profile->PreparedLoadout = FPreparedRaidLoadout();
	Profile->PreparedLoadout.Inventory.GridSize = FIntPoint(LoadoutColumns, LoadoutRows);
	if (!SaveProfile())
	{
		Profile->PreparedLoadout = PreviousPrepared;
		Profile->PendingRaidLoadout = PreviousPending;
		return false;
	}
	return true;
}

bool UProfileSubsystem::ConsumePendingRaidLoadout(FPreparedRaidLoadout& OutLoadout)
{
	if (!Profile || (Profile->PendingRaidLoadout.Inventory.Items.IsEmpty() && Profile->PendingRaidLoadout.Currency == 0))
	{
		return false;
	}
	const FPreparedRaidLoadout Previous = Profile->PendingRaidLoadout;
	OutLoadout = Previous;
	Profile->PendingRaidLoadout = FPreparedRaidLoadout();
	Profile->PendingRaidLoadout.Inventory.GridSize = FIntPoint(LoadoutColumns, LoadoutRows);
	if (!SaveProfile())
	{
		Profile->PendingRaidLoadout = Previous;
		return false;
	}
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
		UE_LOG(LogMYPROJ2Profile, Log, TEXT("Raid death settlement: carried items and currency discarded."));
		return SaveProfile();
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
	Profile->Stash = MoveTemp(StagedStash);
	Profile->StashCurrency += Settlement.CarriedCurrency;
	const bool bSaved = SaveProfile();
	if (!bSaved)
	{
		Profile->Stash = PreviousStash;
		Profile->StashCurrency = PreviousCurrency;
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
