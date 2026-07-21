#include "Loot/LootGenerationLibrary.h"

#include "Engine/AssetManager.h"
#include "Inventory/InventoryGridLibrary.h"
#include "Items/ItemDefinition.h"
#include "Loot/LootContainerDefinition.h"
#include "Loot/LootTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogMYPROJ2Loot, Log, All);

namespace
{
	struct FCandidate
	{
		FName RowName;
		const FLootTableRow* Row = nullptr;
		UItemDefinition* Item = nullptr;
	};

	FIntPoint GetFootprint(const FItemInstance& Item)
	{
		if (UItemDefinition* Definition = FLootGenerationLibrary::ResolveItemDefinition(Item.DefinitionId))
		{
			return Definition->GetEffectiveGridSize(Item.bRotated);
		}
		return FIntPoint(1, 1);
	}

	int32 GetOccupiedCellCount(const TArray<FItemInstance>& Items)
	{
		int32 OccupiedCells = 0;
		for (const FItemInstance& Item : Items)
		{
			const FIntPoint Footprint = GetFootprint(Item);
			OccupiedCells += Footprint.X * Footprint.Y;
		}
		return OccupiedCells;
	}

	bool FindPlacement(const ULootContainerDefinition& Definition, const TArray<FItemInstance>& Items,
		const FIntPoint& Footprint, int32 ReservedEmptyCells, FIntPoint& OutPosition)
	{
		const int32 GridCells = Definition.GridSize.X * Definition.GridSize.Y;
		const int32 RequiredCells = Footprint.X * Footprint.Y;
		if (GetOccupiedCellCount(Items) + RequiredCells > GridCells - ReservedEmptyCells)
		{
			return false;
		}
		TFunction<const FItemInstance&(const FItemInstance&)> GetItem =
			[](const FItemInstance& Item) -> const FItemInstance& { return Item; };
		TFunction<FIntPoint(const FItemInstance&)> GetSize =
			[](const FItemInstance& Item) { return GetFootprint(Item); };
		return FInventoryGridLibrary::FindFirstFit(Definition.GridSize, Items, GetItem, GetSize, Footprint, OutPosition);
	}

	const FCandidate* PickWeighted(const TArray<FCandidate>& Candidates, FRandomStream& Stream)
	{
		float TotalWeight = 0.0f;
		for (const FCandidate& Candidate : Candidates)
		{
			TotalWeight += Candidate.Row->Weight;
		}
		if (TotalWeight <= 0.0f)
		{
			return nullptr;
		}
		float Pick = Stream.FRandRange(0.0f, TotalWeight);
		for (const FCandidate& Candidate : Candidates)
		{
			Pick -= Candidate.Row->Weight;
			if (Pick <= 0.0f)
			{
				return &Candidate;
			}
		}
		return &Candidates.Last();
	}

	int32 AddCandidate(const ULootContainerDefinition& Definition, const FCandidate& Candidate,
		int32 RequestedQuantity, int32 ReservedEmptyCells, int32 Seed, const FGuid& ContainerId,
		int32& InOutItemSequence, TArray<FItemInstance>& InOutItems)
	{
		int32 QuantityRemaining = RequestedQuantity;
		const int32 MaxStack = FMath::Max(1, Candidate.Item->MaxStack);
		for (FItemInstance& Existing : InOutItems)
		{
			if (Existing.DefinitionId != Candidate.Row->ItemDefinitionId || Existing.Quantity >= MaxStack)
			{
				continue;
			}
			const int32 Moved = FMath::Min(MaxStack - Existing.Quantity, QuantityRemaining);
			Existing.Quantity += Moved;
			QuantityRemaining -= Moved;
			if (QuantityRemaining <= 0)
			{
				return RequestedQuantity;
			}
		}

		const FIntPoint Footprint = Candidate.Item->GetEffectiveGridSize(false);
		while (QuantityRemaining > 0)
		{
			FIntPoint Position;
			if (!FindPlacement(Definition, InOutItems, Footprint, ReservedEmptyCells, Position))
			{
				break;
			}
			FItemInstance& NewItem = InOutItems.AddDefaulted_GetRef();
			const int32 Sequence = ++InOutItemSequence;
			NewItem.InstanceId = FGuid(
				HashCombine(Seed, Sequence),
				HashCombine(ContainerId.A, Sequence + 17),
				HashCombine(ContainerId.B, Sequence + 31),
				HashCombine(ContainerId.C ^ ContainerId.D, Sequence + 47));
			NewItem.DefinitionId = Candidate.Row->ItemDefinitionId;
			NewItem.Quantity = FMath::Min(QuantityRemaining, MaxStack);
			NewItem.GridPosition = Position;
			QuantityRemaining -= NewItem.Quantity;
		}
		return RequestedQuantity - QuantityRemaining;
	}
}

int32 FLootGenerationLibrary::MakeSeed(int32 RaidSeed, const FGuid& ContainerId, int32 GenerationVersion)
{
	uint32 Hash = GetTypeHash(RaidSeed);
	Hash = HashCombine(Hash, GetTypeHash(ContainerId));
	Hash = HashCombine(Hash, GetTypeHash(GenerationVersion));
	return static_cast<int32>(Hash);
}

UItemDefinition* FLootGenerationLibrary::ResolveItemDefinition(const FPrimaryAssetId& DefinitionId)
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

bool FLootGenerationLibrary::Generate(const ULootContainerDefinition& Definition, int32 RaidSeed,
	const FGuid& ContainerId, int32 GenerationVersion, float MapValueMultiplier,
	TArray<FItemInstance>& OutItems)
{
	OutItems.Reset();
	if (!ContainerId.IsValid() || !Definition.LootTable || Definition.LootPoolId.IsNone())
	{
		return false;
	}

	TArray<FName> RowNames = Definition.LootTable->GetRowNames();
	RowNames.Sort(FNameLexicalLess());
	TArray<FCandidate> NormalCandidates;
	TArray<FCandidate> ExplicitFillCandidates;
	TArray<FCandidate> OneCellFallbackCandidates;
	for (const FName& RowName : RowNames)
	{
		const FLootTableRow* Row = Definition.LootTable->FindRow<FLootTableRow>(RowName, TEXT("LootGeneration"), false);
		UItemDefinition* Item = Row ? ResolveItemDefinition(Row->ItemDefinitionId) : nullptr;
		const bool bTagMatch = Row && Definition.ZoneTags.HasAll(Row->RequiredZoneTags);
		const bool bInvalid = !Row || !Item || Row->Weight <= 0.0f || Row->MinQuantity <= 0 ||
			Row->MaxQuantity < Row->MinQuantity || Item->GridSize.X > Definition.GridSize.X || Item->GridSize.Y > Definition.GridSize.Y;
		if (bInvalid)
		{
			static TSet<FString> ReportedInvalidRows;
			const FString ReportKey = Definition.LootTable->GetPathName() + TEXT(":") + RowName.ToString();
			if (!ReportedInvalidRows.Contains(ReportKey))
			{
				ReportedInvalidRows.Add(ReportKey);
				UE_LOG(LogMYPROJ2Loot, Warning, TEXT("Skipping invalid loot row '%s' in '%s'."), *RowName.ToString(), *Definition.LootTable->GetName());
			}
			continue;
		}
		if (Row->LootPoolId != Definition.LootPoolId || !bTagMatch)
		{
			continue;
		}
		const FCandidate Candidate{ RowName, Row, Item };
		if (Row->bCanFillValueGap)
		{
			if (Item->GridSize == FIntPoint(1, 1))
			{
				ExplicitFillCandidates.Add(Candidate);
			}
			else
			{
				UE_LOG(LogMYPROJ2Loot, Warning, TEXT("Ignoring non-1x1 value-gap filler row '%s'."), *RowName.ToString());
			}
			continue;
		}
		NormalCandidates.Add(Candidate);
		if (Item->GridSize == FIntPoint(1, 1))
		{
			OneCellFallbackCandidates.Add(Candidate);
		}
	}
	if (NormalCandidates.IsEmpty() && ExplicitFillCandidates.IsEmpty())
	{
		return true;
	}

	const int32 Seed = MakeSeed(RaidSeed, ContainerId, GenerationVersion);
	FRandomStream Stream(Seed);
	const int32 TargetValue = FMath::CeilToInt(FMath::Max(0.0f, Definition.BaseTargetValue * FMath::Max(0.0f, MapValueMultiplier)));
	const int32 GridCells = Definition.GridSize.X * Definition.GridSize.Y;
	const int32 ReservedEmptyCells = FMath::Clamp(Definition.ReservedEmptyCells, 0, FMath::Max(0, GridCells - 1));
	const int32 MaxAttempts = FMath::Clamp(Definition.MaxGenerationAttempts, 1, 256);
	int32 TotalValue = 0;
	int32 ItemSequence = 0;

	for (int32 Attempt = 0; Attempt < MaxAttempts && TotalValue < TargetValue && !NormalCandidates.IsEmpty(); ++Attempt)
	{
		const FCandidate* Candidate = PickWeighted(NormalCandidates, Stream);
		if (!Candidate)
		{
			break;
		}
		const int32 Quantity = Stream.RandRange(Candidate->Row->MinQuantity, Candidate->Row->MaxQuantity);
		const int32 Added = AddCandidate(Definition, *Candidate, Quantity, ReservedEmptyCells, Seed, ContainerId, ItemSequence, OutItems);
		TotalValue += Added * FMath::Max(0, Candidate->Item->LootValue);
	}

	// Only when ordinary shapes cannot reach the lower bound do compact high-value rows fill holes.
	const TArray<FCandidate>& FillCandidates = ExplicitFillCandidates.IsEmpty() ? OneCellFallbackCandidates : ExplicitFillCandidates;
	for (int32 Attempt = 0; Attempt < MaxAttempts && TotalValue < TargetValue && !FillCandidates.IsEmpty(); ++Attempt)
	{
		const FCandidate* Candidate = PickWeighted(FillCandidates, Stream);
		if (!Candidate)
		{
			break;
		}
		const int32 Quantity = Stream.RandRange(Candidate->Row->MinQuantity, Candidate->Row->MaxQuantity);
		const int32 Added = AddCandidate(Definition, *Candidate, Quantity, ReservedEmptyCells, Seed, ContainerId, ItemSequence, OutItems);
		if (Added <= 0)
		{
			break;
		}
		TotalValue += Added * FMath::Max(0, Candidate->Item->LootValue);
	}

	return true;
}
