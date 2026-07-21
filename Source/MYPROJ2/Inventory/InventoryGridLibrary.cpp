#include "Inventory/InventoryGridLibrary.h"

bool FInventoryGridLibrary::IsInBounds(const FIntPoint& GridSize, const FIntPoint& Position, const FIntPoint& Footprint)
{
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return false;
	}
	if (Position.X < 0 || Position.Y < 0)
	{
		return false;
	}
	// Bounds are exclusive at GridSize.X / GridSize.Y.
	if (Position.X + Footprint.X > GridSize.X)
	{
		return false;
	}
	if (Position.Y + Footprint.Y > GridSize.Y)
	{
		return false;
	}
	return true;
}

bool FInventoryGridLibrary::FootprintsOverlap(
	const FIntPoint& PosA, const FIntPoint& SizeA,
	const FIntPoint& PosB, const FIntPoint& SizeB)
{
	// AABB overlap test. Cells are exclusive on the far edge.
	if (PosA.X + SizeA.X <= PosB.X) return false;
	if (PosB.X + SizeB.X <= PosA.X) return false;
	if (PosA.Y + SizeA.Y <= PosB.Y) return false;
	if (PosB.Y + SizeB.Y <= PosA.Y) return false;
	return true;
}

// WouldOverlap and FindFirstFit are templates defined in the header.

bool FInventoryGridLibrary::ComputeMerge(
	int32 SourceQty, int32 TargetQty, int32 MaxStack,
	int32& OutMovedQty, int32& OutNewSourceQty, int32& OutNewTargetQty)
{
	OutMovedQty = 0;
	OutNewSourceQty = SourceQty;
	OutNewTargetQty = TargetQty;

	if (SourceQty <= 0 || MaxStack <= 0)
	{
		return false;
	}
	if (TargetQty >= MaxStack)
	{
		return false;
	}
	const int32 Available = MaxStack - TargetQty;
	const int32 Moved = FMath::Min(Available, SourceQty);
	if (Moved <= 0)
	{
		return false;
	}

	OutMovedQty = Moved;
	OutNewSourceQty = SourceQty - Moved;
	OutNewTargetQty = TargetQty + Moved;
	return true;
}
