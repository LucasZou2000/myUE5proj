#pragma once

#include "CoreMinimal.h"
#include "Inventory/InventoryTypes.h"

/**
 * Pure-function grid math for M3 inventory placement. No global state, no
 * UObject access — definitions are passed in as plain dimensions/max-stack
 * values so the algorithms are deterministic and unit-testable.
 *
 * Coordinates are zero-based, top-left origin, +X right, +Y down.
 * Rotated swaps footprint width/height.
 * Bounds are exclusive at GridSize.X / GridSize.Y.
 *
 * An operation either commits completely or leaves the inventory unchanged.
 */
struct MYPROJ2_API FInventoryGridLibrary
{
	/** Definition snapshot passed into pure functions. */
	struct FDefinitionInfo
	{
		FPrimaryAssetId DefinitionId;
		FIntPoint GridSize = FIntPoint(1, 1);
		int32 MaxStack = 1;

		FIntPoint EffectiveSize(bool bRotated) const
		{
			return bRotated ? FIntPoint(GridSize.Y, GridSize.X) : GridSize;
		}
	};

	/** Test if an axis-aligned footprint is fully inside the grid. */
	static bool IsInBounds(const FIntPoint& GridSize, const FIntPoint& Position, const FIntPoint& Footprint);

	/** Test if two footprints overlap (AABB). */
	static bool FootprintsOverlap(
		const FIntPoint& PosA, const FIntPoint& SizeA,
		const FIntPoint& PosB, const FIntPoint& SizeB);

	/**
	 * Test whether placing `Footprint` at `Position` would collide with any
	 * other entry in `Existing`. The entry matching `IgnoreInstanceId` is
	 * excluded from the collision test (used when moving an item).
	 *
	 * `Existing` may be any array of entry types that expose `.Item` as an
	 * `FItemInstance` (e.g. `FReplicatedInventoryEntry`). `GetItem` projects
	 * an entry to its `FItemInstance`.
	 */
	template<typename EntryType>
	static bool WouldOverlap(
		const TArray<EntryType>& Existing,
		const TFunction<const FItemInstance&(const EntryType&)>& GetItem,
		const TFunction<FIntPoint(const FItemInstance&)>& GetEffectiveSize,
		const FIntPoint& Position,
		const FIntPoint& Footprint,
		const FGuid& IgnoreInstanceId)
	{
		for (const EntryType& Entry : Existing)
		{
			const FItemInstance& Item = GetItem(Entry);
			if (!Item.IsValid())
			{
				continue;
			}
			if (Item.InstanceId == IgnoreInstanceId)
			{
				continue;
			}
			const FIntPoint EntrySize = GetEffectiveSize(Item);
			if (FootprintsOverlap(Item.GridPosition, EntrySize, Position, Footprint))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Find the first cell (row-major, top-left origin) where `Footprint` fits
	 * without overlapping any existing item. Returns false when no cell works.
	 */
	template<typename EntryType>
	static bool FindFirstFit(
		const FIntPoint& GridSize,
		const TArray<EntryType>& Existing,
		const TFunction<const FItemInstance&(const EntryType&)>& GetItem,
		const TFunction<FIntPoint(const FItemInstance&)>& GetEffectiveSize,
		const FIntPoint& Footprint,
		FIntPoint& OutPosition)
	{
		if (Footprint.X <= 0 || Footprint.Y <= 0)
		{
			return false;
		}
		if (Footprint.X > GridSize.X || Footprint.Y > GridSize.Y)
		{
			return false;
		}

		// Deterministic row-major scan.
		for (int32 Y = 0; Y + Footprint.Y <= GridSize.Y; ++Y)
		{
			for (int32 X = 0; X + Footprint.X <= GridSize.X; ++X)
			{
				const FIntPoint Candidate(X, Y);
				if (!WouldOverlap(Existing, GetItem, GetEffectiveSize, Candidate, Footprint, FGuid()))
				{
					OutPosition = Candidate;
					return true;
				}
			}
		}
		return false;
	}

	/**
	 * Compute the new quantity when `Source` of `SourceQty` is merged into
	 * `TargetQty` capped at `MaxStack`. `OutMovedQty` is how many actually
	 * moved (may be partial). Returns false if no movement is possible.
	 */
	static bool ComputeMerge(int32 SourceQty, int32 TargetQty, int32 MaxStack,
		int32& OutMovedQty, int32& OutNewSourceQty, int32& OutNewTargetQty);
};
