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
		UItemDefinition* Definition = nullptr;
	};

	FIntPoint GetFootprint(const FItemInstance& Item)
	{
		if (UItemDefinition* Definition = FLootGenerationLibrary::ResolveItemDefinition(Item.DefinitionId))
		{
			return Definition->GetEffectiveGridSize(Item.bRotated);
		}
		return FIntPoint(1, 1);
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
	TArray<FCandidate> Candidates;
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
		if (Row->LootPoolId == Definition.LootPoolId && bTagMatch)
		{
			Candidates.Add({ RowName, Row, Item });
		}
	}
	if (Candidates.IsEmpty())
	{
		return true;
	}

	FRandomStream Stream(MakeSeed(RaidSeed, ContainerId, GenerationVersion));
	const float Variation = Stream.FRandRange(-Definition.TargetValueVariance, Definition.TargetValueVariance);
	float RemainingBudget = FMath::Max(0.0f, Definition.BaseTargetValue * FMath::Max(0.0f, MapValueMultiplier) * (1.0f + Variation));
	const int32 Rolls = FMath::Clamp(Definition.Rolls, 1, 16);
	TFunction<const FItemInstance&(const FItemInstance&)> GetItem = [](const FItemInstance& Item) -> const FItemInstance& { return Item; };
	TFunction<FIntPoint(const FItemInstance&)> GetSize = [](const FItemInstance& Item) { return GetFootprint(Item); };

	for (int32 Roll = 0; Roll < Rolls; ++Roll)
	{
		float TotalWeight = 0.0f;
		TArray<float> EffectiveWeights;
		for (const FCandidate& Candidate : Candidates)
		{
			const float UnitValue = FMath::Max(1, Candidate.Definition->LootValue);
			// Bias toward entries that consume the remaining budget, while retaining a floor for surprises.
			const float BudgetFit = FMath::Clamp(RemainingBudget / UnitValue, 0.20f, 2.0f);
			const float Weight = Candidate.Row->Weight * BudgetFit;
			EffectiveWeights.Add(Weight);
			TotalWeight += Weight;
		}
		if (TotalWeight <= 0.0f)
		{
			break;
		}

		float Pick = Stream.FRandRange(0.0f, TotalWeight);
		int32 PickedIndex = 0;
		for (; PickedIndex < EffectiveWeights.Num() - 1; ++PickedIndex)
		{
			Pick -= EffectiveWeights[PickedIndex];
			if (Pick <= 0.0f)
			{
				break;
			}
		}
		const FCandidate& Candidate = Candidates[PickedIndex];
		const int32 RolledQuantity = Stream.RandRange(Candidate.Row->MinQuantity, Candidate.Row->MaxQuantity);
		int32 QuantityRemaining = RolledQuantity;
		const FIntPoint Footprint = Candidate.Definition->GetEffectiveGridSize(false);
		const int32 MaxStack = FMath::Max(1, Candidate.Definition->MaxStack);
		for (FItemInstance& Existing : OutItems)
		{
			if (Existing.DefinitionId != Candidate.Row->ItemDefinitionId || QuantityRemaining <= 0)
			{
				continue;
			}
			const int32 Moved = FMath::Min(MaxStack - Existing.Quantity, QuantityRemaining);
			Existing.Quantity += Moved;
			QuantityRemaining -= Moved;
		}
		while (QuantityRemaining > 0)
		{
			FIntPoint Position;
			if (!FInventoryGridLibrary::FindFirstFit(Definition.GridSize, OutItems, GetItem, GetSize, Footprint, Position))
			{
				break;
			}
			FItemInstance& NewItem = OutItems.AddDefaulted_GetRef();
			NewItem.InstanceId = FGuid(
				HashCombine(MakeSeed(RaidSeed, ContainerId, GenerationVersion), Roll + 1 + OutItems.Num()),
				HashCombine(ContainerId.A, Roll + 17), HashCombine(ContainerId.B, Roll + 31), HashCombine(ContainerId.C ^ ContainerId.D, Roll + 47));
			NewItem.DefinitionId = Candidate.Row->ItemDefinitionId;
			NewItem.Quantity = FMath::Min(QuantityRemaining, MaxStack);
			NewItem.GridPosition = Position;
			QuantityRemaining -= NewItem.Quantity;
		}
		const int32 QuantityAdded = RolledQuantity - QuantityRemaining;
		RemainingBudget = FMath::Max(0.0f, RemainingBudget - QuantityAdded * Candidate.Definition->LootValue);
	}

	return true;
}
