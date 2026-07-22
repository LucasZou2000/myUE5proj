#include "Inventory/InventoryComponent.h"

#include "Character/MYPROJ2CharacterBase.h"
#include "Combat/HealthComponent.h"
#include "Engine/AssetManager.h"
#include "Inventory/InventoryGridLibrary.h"
#include "Items/ItemDefinition.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogMYPROJ2Inventory, Log, All);

// ---------------------------------------------------------------------------
// Fast Array entry hooks (presentation only)
// ---------------------------------------------------------------------------

void FReplicatedInventoryEntry::PostReplicatedAdd(const FReplicatedInventoryList& InArraySerializer)
{
	if (UInventoryComponent* Owner = InArraySerializer.OwnerComponent.Get())
	{
		Owner->OnRep_EntriesChanged();
	}
}

void FReplicatedInventoryEntry::PostReplicatedChange(const FReplicatedInventoryList& InArraySerializer)
{
	if (UInventoryComponent* Owner = InArraySerializer.OwnerComponent.Get())
	{
		Owner->OnRep_EntriesChanged();
	}
}

void FReplicatedInventoryEntry::PreReplicatedRemove(const FReplicatedInventoryList& InArraySerializer)
{
	if (UInventoryComponent* Owner = InArraySerializer.OwnerComponent.Get())
	{
		Owner->OnRep_EntriesChanged();
	}
}

// ---------------------------------------------------------------------------
// UInventoryComponent
// ---------------------------------------------------------------------------

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedItems.SetOwner(this);

	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && bSeedTestItems)
	{
		for (TObjectPtr<UItemDefinition>& Def : SeedItems)
		{
			if (Def)
			{
				const int32 GrantQty = FMath::Max(1, Def->MaxStack);
				AuthorityTryAdd(Def, GrantQty);
			}
		}
	}
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The component lives on the PlayerController, which itself replicates only
	// to the owning client. No additional condition filter needed.
	DOREPLIFETIME(UInventoryComponent, ReplicatedItems);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

const FReplicatedInventoryEntry* UInventoryComponent::FindEntry(const FGuid& InstanceId) const
{
	if (!InstanceId.IsValid())
	{
		return nullptr;
	}
	for (const FReplicatedInventoryEntry& Entry : ReplicatedItems.Entries)
	{
		if (Entry.Item.InstanceId == InstanceId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

FReplicatedInventoryEntry* UInventoryComponent::FindEntryMutable(const FGuid& InstanceId)
{
	if (!InstanceId.IsValid())
	{
		return nullptr;
	}
	for (FReplicatedInventoryEntry& Entry : ReplicatedItems.Entries)
	{
		if (Entry.Item.InstanceId == InstanceId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

UItemDefinition* UInventoryComponent::ResolveDefinition(const FPrimaryAssetId& DefinitionId) const
{
	if (!DefinitionId.IsValid())
	{
		return nullptr;
	}
	// Scanning registers an ID-to-path mapping, but it does not load the asset.
	// Replicated entries carry only the ID, so load its definition on demand for
	// the owning client when it has not already been loaded.
	UAssetManager& AM = UAssetManager::Get();
	if (UItemDefinition* LoadedDefinition = Cast<UItemDefinition>(AM.GetPrimaryAssetObject(DefinitionId)))
	{
		return LoadedDefinition;
	}

	const FSoftObjectPath DefinitionPath = AM.GetPrimaryAssetPath(DefinitionId);
	static TSet<FPrimaryAssetId> ReportedDefinitionFailures;
	const bool bFirstFailureForId = !ReportedDefinitionFailures.Contains(DefinitionId);
	ReportedDefinitionFailures.Add(DefinitionId);
	if (!DefinitionPath.IsValid())
	{
		if (bFirstFailureForId)
		{
			TArray<FPrimaryAssetId> RegisteredItemIds;
			AM.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("Item")), RegisteredItemIds);
			UE_LOG(LogMYPROJ2Inventory, Warning,
				TEXT("Definition '%s' is not registered. AssetManager Item IDs: %s"),
				*DefinitionId.ToString(), *FString::JoinBy(RegisteredItemIds, TEXT(", "),
					[](const FPrimaryAssetId& Id) { return Id.ToString(); }));
		}
		return nullptr;
	}

	UItemDefinition* LoadedDefinition = Cast<UItemDefinition>(DefinitionPath.TryLoad());
	if (!LoadedDefinition && bFirstFailureForId)
	{
		UE_LOG(LogMYPROJ2Inventory, Warning, TEXT("Could not load inventory definition '%s' from '%s'."),
			*DefinitionId.ToString(), *DefinitionPath.ToString());
	}
	return LoadedDefinition;
}

FIntPoint UInventoryComponent::GetEntryFootprint(const FItemInstance& Entry) const
{
	if (UItemDefinition* Def = ResolveDefinition(Entry.DefinitionId))
	{
		return Def->GetEffectiveGridSize(Entry.bRotated);
	}
	return FIntPoint(1, 1);
}

void UInventoryComponent::OnRep_EntriesChanged()
{
	OnInventoryChanged.Broadcast();
}

FString UInventoryComponent::GetInventoryDisplayText() const
{
	TArray<FString> Lines = GetInventoryEntryStrings();
	if (Lines.Num() == 0)
	{
		return TEXT("Inventory is empty");
	}
	return FString::Join(Lines, TEXT("\n"));
}

TArray<FString> UInventoryComponent::GetInventoryEntryStrings() const
{
	TArray<FString> Result;
	Result.Reserve(ReplicatedItems.Entries.Num());

	for (int32 Index = 0; Index < ReplicatedItems.Entries.Num(); ++Index)
	{
		const FReplicatedInventoryEntry& Entry = ReplicatedItems.Entries[Index];
		if (!Entry.Item.IsValid())
		{
			continue;
		}

		FString DefName = Entry.Item.DefinitionId.PrimaryAssetName.ToString();
		FString SizeStr = TEXT("?x?");
		if (UItemDefinition* Def = ResolveDefinition(Entry.Item.DefinitionId))
		{
			const FIntPoint Size = Def->GetEffectiveGridSize(Entry.Item.bRotated);
			SizeStr = FString::Printf(TEXT("%dx%d"), Size.X, Size.Y);
			DefName = Def->DisplayName.IsEmpty() ? DefName : Def->DisplayName.ToString();
		}

		Result.Add(FString::Printf(TEXT("[%d] %s x%d (%s) at (%d,%d)%s"),
			Index,
			*DefName,
			Entry.Item.Quantity,
			*SizeStr,
			Entry.Item.GridPosition.X,
			Entry.Item.GridPosition.Y,
			Entry.Item.bRotated ? TEXT(" [R]") : TEXT("")));
	}

	return Result;
}

void UInventoryComponent::DebugGrantSeedItems()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogMYPROJ2Inventory, Warning, TEXT("DebugGrantSeedItems: not authority, ignoring"));
		return;
	}

	if (SeedItems.Num() == 0)
	{
		UE_LOG(LogMYPROJ2Inventory, Warning, TEXT("DebugGrantSeedItems: SeedItems is empty"));
		return;
	}

	for (TObjectPtr<UItemDefinition>& Def : SeedItems)
	{
		if (Def)
		{
			const int32 GrantQty = FMath::Max(1, Def->MaxStack);
			const bool bOk = AuthorityTryAdd(Def, GrantQty);
			UE_LOG(LogMYPROJ2Inventory, Log, TEXT("DebugGrantSeedItems: %s x%d -> %s"),
				*Def->ItemId.ToString(), GrantQty, bOk ? TEXT("OK") : TEXT("FAILED"));
		}
	}
}

// ---------------------------------------------------------------------------
// Request id bookkeeping
// ---------------------------------------------------------------------------

bool UInventoryComponent::IsRequestStale(uint16 RequestId) const
{
	if (!bHasConsumedRequest)
	{
		return false;
	}
	// Wrap-safe comparison: stale when RequestId <= LastConsumedRequestId in
	// modular arithmetic. Treat distance > 32768 as "in the future" (new).
	const int32 Delta = static_cast<int32>(RequestId) - static_cast<int32>(LastConsumedRequestId);
	const int32 WrappedDelta = ((Delta + 32768) % 65536 + 65536) % 65536 - 32768;
	return WrappedDelta <= 0;
}

void UInventoryComponent::MarkRequestConsumed(uint16 RequestId)
{
	LastConsumedRequestId = RequestId;
	bHasConsumedRequest = true;
}

// ---------------------------------------------------------------------------
// Commit helpers
// ---------------------------------------------------------------------------

void UInventoryComponent::CommitNewEntry(FItemInstance&& NewItem)
{
	FReplicatedInventoryEntry& Entry = ReplicatedItems.Entries.AddDefaulted_GetRef();
	Entry.Item = MoveTemp(NewItem);
	Entry.OwnerComponent = this;
	ReplicatedItems.MarkItemDirty(Entry);
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::MarkEntryDirty(FReplicatedInventoryEntry& Entry)
{
	ReplicatedItems.MarkItemDirty(Entry);
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::RemoveEntry(const FGuid& InstanceId)
{
	for (int32 Index = 0; Index < ReplicatedItems.Entries.Num(); ++Index)
	{
		if (ReplicatedItems.Entries[Index].Item.InstanceId == InstanceId)
		{
			ReplicatedItems.Entries.RemoveAt(Index);
			ReplicatedItems.MarkArrayDirty();
			OnInventoryChanged.Broadcast();
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Authority-only mutators
// ---------------------------------------------------------------------------

bool UInventoryComponent::AuthorityTryAdd(UItemDefinition* Definition, int32 Quantity)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Definition || Quantity <= 0)
	{
		return false;
	}

	const FIntPoint GridSize(GridColumns, GridRows);
	const FPrimaryAssetId DefId = Definition->GetPrimaryAssetId();
	const int32 MaxStack = FMath::Max(1, Definition->MaxStack);

	int32 Remaining = Quantity;

	// Pass 1: top up existing compatible stacks.
	for (int32 Index = 0; Index < ReplicatedItems.Entries.Num() && Remaining > 0; ++Index)
	{
		FReplicatedInventoryEntry& Entry = ReplicatedItems.Entries[Index];
		if (Entry.Item.DefinitionId != DefId)
		{
			continue;
		}
		if (Entry.Item.Quantity >= MaxStack)
		{
			continue;
		}
		int32 MovedQty = 0, NewSrcQty = 0, NewTgtQty = 0;
		if (FInventoryGridLibrary::ComputeMerge(Remaining, Entry.Item.Quantity, MaxStack, MovedQty, NewSrcQty, NewTgtQty))
		{
			Entry.Item.Quantity = NewTgtQty;
			MarkEntryDirty(Entry);
			Remaining = NewSrcQty;
		}
	}

	// Pass 2: place new entries first-fit.
	TFunction<const FItemInstance&(const FReplicatedInventoryEntry&)> GetItem =
		[](const FReplicatedInventoryEntry& E) -> const FItemInstance& { return E.Item; };
	TFunction<FIntPoint(const FItemInstance&)> GetFootprint =
		[this](const FItemInstance& E) { return GetEntryFootprint(E); };
	while (Remaining > 0)
	{
		FIntPoint Position;
		const FIntPoint Footprint = Definition->GetEffectiveGridSize(false);
		if (!FInventoryGridLibrary::FindFirstFit(GridSize, ReplicatedItems.Entries,
			GetItem, GetFootprint, Footprint, Position))
		{
			return Remaining == 0;
		}

		FItemInstance NewItem;
		NewItem.InstanceId = FGuid::NewGuid();
		NewItem.DefinitionId = DefId;
		NewItem.Quantity = FMath::Min(Remaining, MaxStack);
		NewItem.GridPosition = Position;
		NewItem.bRotated = false;

		Remaining -= NewItem.Quantity;
		CommitNewEntry(MoveTemp(NewItem));
	}

	return true;
}

bool UInventoryComponent::AuthorityTryRemove(const FGuid& InstanceId, int32 Quantity)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Quantity <= 0)
	{
		return false;
	}

	FReplicatedInventoryEntry* Entry = FindEntryMutable(InstanceId);
	if (!Entry || Entry->Item.Quantity < Quantity)
	{
		return false;
	}

	Entry->Item.Quantity -= Quantity;
	if (Entry->Item.Quantity <= 0)
	{
		RemoveEntry(InstanceId);
	}
	else
	{
		MarkEntryDirty(*Entry);
	}
	return true;
}

bool UInventoryComponent::AuthorityTransferTo(UInventoryComponent* Destination, const FGuid& InstanceId,
	int32 Quantity, const TOptional<FIntPoint>& TargetPosition, bool bRotated, EInventoryRejectReason& OutReason)
{
	OutReason = EInventoryRejectReason::InvalidRequest;
	AActor* SourceOwner = GetOwner();
	AActor* DestinationOwner = Destination ? Destination->GetOwner() : nullptr;
	if (!SourceOwner || !DestinationOwner || !SourceOwner->HasAuthority() || !DestinationOwner->HasAuthority() ||
		Destination == this || Quantity <= 0)
	{
		return false;
	}

	FReplicatedInventoryEntry* Source = FindEntryMutable(InstanceId);
	if (!Source || Source->Item.Quantity < Quantity)
	{
		OutReason = EInventoryRejectReason::MissingItem;
		return false;
	}
	UItemDefinition* Definition = ResolveDefinition(Source->Item.DefinitionId);
	if (!Definition)
	{
		OutReason = EInventoryRejectReason::InvalidDefinition;
		return false;
	}

	// Stage the destination as plain items. No Fast Array mutation or delegate can
	// occur before all quantities and placements have been proven valid.
	TArray<FItemInstance> StagedItems;
	StagedItems.Reserve(Destination->ReplicatedItems.Entries.Num() + 2);
	for (const FReplicatedInventoryEntry& Entry : Destination->ReplicatedItems.Entries)
	{
		StagedItems.Add(Entry.Item);
	}
	int32 Remaining = Quantity;
	const int32 MaxStack = FMath::Max(1, Definition->MaxStack);
	for (FItemInstance& Entry : StagedItems)
	{
		if (Entry.DefinitionId != Source->Item.DefinitionId || Remaining <= 0)
		{
			continue;
		}
		const int32 Space = MaxStack - Entry.Quantity;
		const int32 Moved = FMath::Min(Space, Remaining);
		Entry.Quantity += Moved;
		Remaining -= Moved;
	}

	const FIntPoint Footprint = Definition->GetEffectiveGridSize(bRotated);
	TFunction<const FItemInstance&(const FItemInstance&)> GetItem = [](const FItemInstance& Item) -> const FItemInstance& { return Item; };
	TFunction<FIntPoint(const FItemInstance&)> GetFootprint = [Destination](const FItemInstance& Item)
	{
		return Destination->GetEntryFootprint(Item);
	};
	bool bUsedExplicitPosition = false;
	while (Remaining > 0)
	{
		const int32 StackQuantity = FMath::Min(Remaining, MaxStack);
		FIntPoint Position;
		if (TargetPosition.IsSet() && !bUsedExplicitPosition)
		{
			Position = TargetPosition.GetValue();
			if (!FInventoryGridLibrary::IsInBounds(Destination->GetGridSize(), Position, Footprint) ||
				FInventoryGridLibrary::WouldOverlap(StagedItems, GetItem, GetFootprint, Position, Footprint, FGuid()))
			{
				OutReason = EInventoryRejectReason::NoSpace;
				return false;
			}
			bUsedExplicitPosition = true;
		}
		else if (!FInventoryGridLibrary::FindFirstFit(Destination->GetGridSize(), StagedItems, GetItem, GetFootprint, Footprint, Position))
		{
			OutReason = EInventoryRejectReason::NoSpace;
			return false;
		}
		FItemInstance NewItem;
		NewItem.InstanceId = FGuid::NewGuid();
		NewItem.DefinitionId = Source->Item.DefinitionId;
		NewItem.Quantity = StackQuantity;
		NewItem.GridPosition = Position;
		NewItem.bRotated = bRotated;
		StagedItems.Add(NewItem);
		Remaining -= StackQuantity;
	}

	// Commit source and destination back-to-back. Broadcast only after both are durable.
	Source->Item.Quantity -= Quantity;
	if (Source->Item.Quantity == 0)
	{
		for (int32 Index = 0; Index < ReplicatedItems.Entries.Num(); ++Index)
		{
			if (ReplicatedItems.Entries[Index].Item.InstanceId == InstanceId)
			{
				ReplicatedItems.Entries.RemoveAt(Index);
				ReplicatedItems.MarkArrayDirty();
				break;
			}
		}
	}
	else
	{
		ReplicatedItems.MarkItemDirty(*Source);
	}

	Destination->ReplicatedItems.Entries.Reset();
	for (FItemInstance& Item : StagedItems)
	{
		FReplicatedInventoryEntry& Entry = Destination->ReplicatedItems.Entries.AddDefaulted_GetRef();
		Entry.Item = MoveTemp(Item);
		Entry.OwnerComponent = Destination;
	}
	Destination->ReplicatedItems.MarkArrayDirty();
	OnInventoryChanged.Broadcast();
	Destination->OnInventoryChanged.Broadcast();
	OutReason = EInventoryRejectReason::None;
	return true;
}

bool UInventoryComponent::AuthorityReplaceAll(const TArray<FItemInstance>& Items, EInventoryRejectReason& OutReason)
{
	OutReason = EInventoryRejectReason::InvalidRequest;
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}
	TArray<FItemInstance> Staged;
	TSet<FGuid> SeenIds;
	TFunction<const FItemInstance&(const FItemInstance&)> GetItem =
		[](const FItemInstance& Value) -> const FItemInstance& { return Value; };
	TFunction<FIntPoint(const FItemInstance&)> GetFootprint = [this](const FItemInstance& Value)
	{
		return GetEntryFootprint(Value);
	};
	for (const FItemInstance& Item : Items)
	{
		UItemDefinition* Definition = ResolveDefinition(Item.DefinitionId);
		if (!Item.IsValid() || !Definition || Item.Quantity > FMath::Max(1, Definition->MaxStack))
		{
			OutReason = EInventoryRejectReason::InvalidDefinition;
			return false;
		}
		const FIntPoint Footprint = Definition->GetEffectiveGridSize(Item.bRotated);
		if (SeenIds.Contains(Item.InstanceId) || !FInventoryGridLibrary::IsInBounds(GetGridSize(), Item.GridPosition, Footprint) ||
			FInventoryGridLibrary::WouldOverlap(Staged, GetItem, GetFootprint, Item.GridPosition, Footprint, FGuid()))
		{
			OutReason = EInventoryRejectReason::NoSpace;
			return false;
		}
		SeenIds.Add(Item.InstanceId);
		Staged.Add(Item);
	}
	ReplicatedItems.Entries.Reset();
	for (const FItemInstance& Item : Staged)
	{
		FReplicatedInventoryEntry& Entry = ReplicatedItems.Entries.AddDefaulted_GetRef();
		Entry.Item = Item;
		Entry.OwnerComponent = this;
	}
	ReplicatedItems.MarkArrayDirty();
	OnInventoryChanged.Broadcast();
	OutReason = EInventoryRejectReason::None;
	return true;
}

void UInventoryComponent::AuthorityClearAll()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	if (!ReplicatedItems.Entries.IsEmpty())
	{
		ReplicatedItems.Entries.Reset();
		ReplicatedItems.MarkArrayDirty();
		OnInventoryChanged.Broadcast();
	}
}

bool UInventoryComponent::AuthorityMoveItem(const FGuid& ItemId, FIntPoint Position, bool bRotated, EInventoryRejectReason& OutReason)
{
	FReplicatedInventoryEntry* Entry = FindEntryMutable(ItemId);
	if (!Entry)
	{
		OutReason = EInventoryRejectReason::MissingItem;
		return false;
	}

	UItemDefinition* Def = ResolveDefinition(Entry->Item.DefinitionId);
	if (!Def)
	{
		OutReason = EInventoryRejectReason::InvalidDefinition;
		return false;
	}

	const FIntPoint GridSize(GridColumns, GridRows);
	const FIntPoint Footprint = Def->GetEffectiveGridSize(bRotated);

	if (!FInventoryGridLibrary::IsInBounds(GridSize, Position, Footprint))
	{
		OutReason = EInventoryRejectReason::OutOfBounds;
		return false;
	}

	TFunction<const FItemInstance&(const FReplicatedInventoryEntry&)> GetItem =
		[](const FReplicatedInventoryEntry& E) -> const FItemInstance& { return E.Item; };
	TFunction<FIntPoint(const FItemInstance&)> GetFootprint =
		[this](const FItemInstance& E) { return GetEntryFootprint(E); };
	if (FInventoryGridLibrary::WouldOverlap(ReplicatedItems.Entries,
		GetItem, GetFootprint, Position, Footprint, ItemId))
	{
		OutReason = EInventoryRejectReason::Overlap;
		return false;
	}

	Entry->Item.GridPosition = Position;
	Entry->Item.bRotated = bRotated;
	MarkEntryDirty(*Entry);
	OutReason = EInventoryRejectReason::None;
	return true;
}

bool UInventoryComponent::AuthoritySplitStack(const FGuid& ItemId, int32 Quantity, FIntPoint Position, bool bRotated, EInventoryRejectReason& OutReason)
{
	if (Quantity <= 0)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}

	FReplicatedInventoryEntry* Source = FindEntryMutable(ItemId);
	if (!Source)
	{
		OutReason = EInventoryRejectReason::MissingItem;
		return false;
	}
	if (Source->Item.Quantity <= Quantity)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}

	UItemDefinition* Def = ResolveDefinition(Source->Item.DefinitionId);
	if (!Def)
	{
		OutReason = EInventoryRejectReason::InvalidDefinition;
		return false;
	}

	const FIntPoint GridSize(GridColumns, GridRows);
	const FIntPoint Footprint = Def->GetEffectiveGridSize(bRotated);

	if (!FInventoryGridLibrary::IsInBounds(GridSize, Position, Footprint))
	{
		OutReason = EInventoryRejectReason::OutOfBounds;
		return false;
	}
	TFunction<const FItemInstance&(const FReplicatedInventoryEntry&)> GetItem =
		[](const FReplicatedInventoryEntry& E) -> const FItemInstance& { return E.Item; };
	TFunction<FIntPoint(const FItemInstance&)> GetFootprint =
		[this](const FItemInstance& E) { return GetEntryFootprint(E); };
	if (FInventoryGridLibrary::WouldOverlap(ReplicatedItems.Entries,
		GetItem, GetFootprint, Position, Footprint, ItemId))
	{
		OutReason = EInventoryRejectReason::Overlap;
		return false;
	}

	// Commit: decrement source, insert new entry.
	Source->Item.Quantity -= Quantity;
	MarkEntryDirty(*Source);

	FItemInstance NewItem;
	NewItem.InstanceId = FGuid::NewGuid();
	NewItem.DefinitionId = Source->Item.DefinitionId;
	NewItem.Quantity = Quantity;
	NewItem.GridPosition = Position;
	NewItem.bRotated = bRotated;
	CommitNewEntry(MoveTemp(NewItem));

	OutReason = EInventoryRejectReason::None;
	return true;
}

bool UInventoryComponent::AuthorityMergeStacks(const FGuid& SourceId, const FGuid& TargetId, int32 Quantity, EInventoryRejectReason& OutReason)
{
	if (SourceId == TargetId)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}
	if (Quantity <= 0)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}

	FReplicatedInventoryEntry* Source = FindEntryMutable(SourceId);
	FReplicatedInventoryEntry* Target = FindEntryMutable(TargetId);
	if (!Source || !Target)
	{
		OutReason = EInventoryRejectReason::MissingItem;
		return false;
	}
	if (Source->Item.DefinitionId != Target->Item.DefinitionId)
	{
		OutReason = EInventoryRejectReason::StackMismatch;
		return false;
	}

	UItemDefinition* Def = ResolveDefinition(Source->Item.DefinitionId);
	if (!Def)
	{
		OutReason = EInventoryRejectReason::InvalidDefinition;
		return false;
	}
	const int32 MaxStack = FMath::Max(1, Def->MaxStack);
	if (Target->Item.Quantity >= MaxStack)
	{
		OutReason = EInventoryRejectReason::StackFull;
		return false;
	}

	const int32 Requested = FMath::Min(Quantity, Source->Item.Quantity);
	int32 MovedQty = 0, NewSrcQty = 0, NewTgtQty = 0;
	if (!FInventoryGridLibrary::ComputeMerge(Requested, Target->Item.Quantity, MaxStack, MovedQty, NewSrcQty, NewTgtQty))
	{
		OutReason = EInventoryRejectReason::StackFull;
		return false;
	}

	// Commit. Source may be partially or fully drained.
	const int32 FinalSourceQty = Source->Item.Quantity - MovedQty;
	Target->Item.Quantity = NewTgtQty;
	MarkEntryDirty(*Target);

	if (FinalSourceQty <= 0)
	{
		RemoveEntry(SourceId);
	}
	else
	{
		Source = FindEntryMutable(SourceId); // re-find: RemoveEntry may have shifted
		if (Source)
		{
			Source->Item.Quantity = FinalSourceQty;
			MarkEntryDirty(*Source);
		}
	}

	OutReason = EInventoryRejectReason::None;
	return true;
}

bool UInventoryComponent::AuthorityUseItem(const FGuid& ItemId, EInventoryRejectReason& OutReason)
{
	FReplicatedInventoryEntry* Entry = FindEntryMutable(ItemId);
	if (!Entry)
	{
		OutReason = EInventoryRejectReason::MissingItem;
		return false;
	}

	UItemDefinition* Def = ResolveDefinition(Entry->Item.DefinitionId);
	if (!Def)
	{
		OutReason = EInventoryRejectReason::InvalidDefinition;
		return false;
	}

	UConsumableItemDefinition* Consumable = Cast<UConsumableItemDefinition>(Def);
	if (!Consumable)
	{
		OutReason = EInventoryRejectReason::NotUsable;
		return false;
	}

	// Find the owning pawn's health component. The inventory lives on the
	// PlayerController, so we go through its pawn.
	AActor* OwnerActor = GetOwner();
	AController* Controller = Cast<AController>(OwnerActor);
	APawn* Pawn = nullptr;
	if (Controller)
	{
		Pawn = Controller->GetPawn();
	}
	else
	{
		Pawn = Cast<APawn>(OwnerActor);
	}
	if (!Pawn)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}
	UHealthComponent* Health = Pawn->FindComponentByClass<UHealthComponent>();
	if (!Health)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}
	if (Health->IsDead())
	{
		OutReason = EInventoryRejectReason::Dead;
		return false;
	}
	if (Consumable->HealAmount > 0.f && Health->GetHealth() >= Health->GetMaxHealth())
	{
		OutReason = EInventoryRejectReason::FullHealth;
		return false;
	}

	// Apply effect first; only consume the item on success.
	if (Consumable->HealAmount > 0.f)
	{
		Health->AuthorityHeal(Consumable->HealAmount);
	}

	Entry->Item.Quantity -= 1;
	if (Entry->Item.Quantity <= 0)
	{
		RemoveEntry(ItemId);
	}
	else
	{
		MarkEntryDirty(*Entry);
	}

	OutReason = EInventoryRejectReason::None;
	return true;
}

// ---------------------------------------------------------------------------
// RPCs
// ---------------------------------------------------------------------------

void UInventoryComponent::ServerMoveItem_Implementation(FGuid ItemId, FIntPoint Position, bool bRotated, uint16 RequestId)
{
	if (IsRequestStale(RequestId))
	{
		ClientInventoryRejected(RequestId, EInventoryRejectReason::StaleRequest);
		return;
	}
	EInventoryRejectReason Reason = EInventoryRejectReason::None;
	if (!AuthorityMoveItem(ItemId, Position, bRotated, Reason))
	{
		ClientInventoryRejected(RequestId, Reason);
		return;
	}
	MarkRequestConsumed(RequestId);
}

void UInventoryComponent::ServerSplitStack_Implementation(FGuid ItemId, int32 Quantity, FIntPoint Position, bool bRotated, uint16 RequestId)
{
	if (IsRequestStale(RequestId))
	{
		ClientInventoryRejected(RequestId, EInventoryRejectReason::StaleRequest);
		return;
	}
	EInventoryRejectReason Reason = EInventoryRejectReason::None;
	if (!AuthoritySplitStack(ItemId, Quantity, Position, bRotated, Reason))
	{
		ClientInventoryRejected(RequestId, Reason);
		return;
	}
	MarkRequestConsumed(RequestId);
}

void UInventoryComponent::ServerMergeStacks_Implementation(FGuid SourceId, FGuid TargetId, int32 Quantity, uint16 RequestId)
{
	if (IsRequestStale(RequestId))
	{
		ClientInventoryRejected(RequestId, EInventoryRejectReason::StaleRequest);
		return;
	}
	EInventoryRejectReason Reason = EInventoryRejectReason::None;
	if (!AuthorityMergeStacks(SourceId, TargetId, Quantity, Reason))
	{
		ClientInventoryRejected(RequestId, Reason);
		return;
	}
	MarkRequestConsumed(RequestId);
}

void UInventoryComponent::ServerUseItem_Implementation(FGuid ItemId, uint16 RequestId)
{
	if (IsRequestStale(RequestId))
	{
		ClientInventoryRejected(RequestId, EInventoryRejectReason::StaleRequest);
		return;
	}
	EInventoryRejectReason Reason = EInventoryRejectReason::None;
	if (!AuthorityUseItem(ItemId, Reason))
	{
		ClientInventoryRejected(RequestId, Reason);
		return;
	}
	MarkRequestConsumed(RequestId);
}

void UInventoryComponent::ClientInventoryRejected_Implementation(uint16 RequestId, EInventoryRejectReason Reason)
{
	// Owning client: surface for UI to restore the source slot. M3 has no modal
	// dialog; the grid UI simply re-reads the Fast Array (which is unchanged on
	// rejection) and the drag ghost is discarded.
	UE_LOG(LogMYPROJ2Inventory, Verbose, TEXT("Inventory request %hu rejected: reason=%d"),
		RequestId, static_cast<int32>(Reason));
}
