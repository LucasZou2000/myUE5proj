#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/DataTable.h"
#include "Inventory/InventoryGridLibrary.h"
#include "Items/ItemDefinition.h"
#include "Loot/LootContainerDefinition.h"
#include "Loot/LootGenerationLibrary.h"
#include "Loot/LootTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMYPROJ2LootGenerationDeterminismTest,
	"MYPROJ2.Loot.Generation.DeterminismAndBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMYPROJ2LootGenerationDeterminismTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FLootTableRow::StaticStruct();

	FLootTableRow ScrapRow;
	ScrapRow.LootPoolId = TEXT("TestCrate");
	ScrapRow.ItemDefinitionId = FPrimaryAssetId(TEXT("Item"), TEXT("Scrap"));
	ScrapRow.Weight = 70.0f;
	ScrapRow.MinQuantity = 3;
	ScrapRow.MaxQuantity = 8;
	Table->AddRow(TEXT("Scrap"), ScrapRow);

	FLootTableRow MedkitRow;
	MedkitRow.LootPoolId = TEXT("TestCrate");
	MedkitRow.ItemDefinitionId = FPrimaryAssetId(TEXT("Item"), TEXT("Medkit"));
	MedkitRow.Weight = 30.0f;
	MedkitRow.MinQuantity = 1;
	MedkitRow.MaxQuantity = 2;
	Table->AddRow(TEXT("Medkit"), MedkitRow);

	ULootContainerDefinition* ContainerDefinition = NewObject<ULootContainerDefinition>();
	ContainerDefinition->LootPoolId = TEXT("TestCrate");
	ContainerDefinition->MaxGenerationAttempts = 64;
	ContainerDefinition->LootTable = Table;
	ContainerDefinition->GridSize = FIntPoint(5, 8);
	ContainerDefinition->BaseTargetValue = 180;
	ContainerDefinition->ReservedEmptyCells = 4;

	const FGuid ContainerId(0x10203040, 0x50607080, 0x90A0B0C0, 0xD0E0F001);
	TArray<FItemInstance> First;
	TArray<FItemInstance> Second;
	TestTrue(TEXT("First generation succeeds"),
		FLootGenerationLibrary::Generate(*ContainerDefinition, 123456, ContainerId, 1, 1.0f, First));
	TestTrue(TEXT("Second generation succeeds"),
		FLootGenerationLibrary::Generate(*ContainerDefinition, 123456, ContainerId, 1, 1.0f, Second));
	TestEqual(TEXT("Generated entry count is deterministic"), First.Num(), Second.Num());
	TestTrue(TEXT("Test table produces at least one entry"), First.Num() > 0);

	int32 TotalValue = 0;
	int32 OccupiedCells = 0;
	for (int32 Index = 0; Index < First.Num() && Index < Second.Num(); ++Index)
	{
		const FItemInstance& A = First[Index];
		const FItemInstance& B = Second[Index];
		TestEqual(FString::Printf(TEXT("Definition %d"), Index), A.DefinitionId, B.DefinitionId);
		TestEqual(FString::Printf(TEXT("Quantity %d"), Index), A.Quantity, B.Quantity);
		TestEqual(FString::Printf(TEXT("Position %d"), Index), A.GridPosition, B.GridPosition);
		TestEqual(FString::Printf(TEXT("Instance id %d"), Index), A.InstanceId, B.InstanceId);

		UItemDefinition* ItemDefinition = FLootGenerationLibrary::ResolveItemDefinition(A.DefinitionId);
		TestNotNull(FString::Printf(TEXT("Definition resolves %d"), Index), ItemDefinition);
		if (ItemDefinition)
		{
			TotalValue += A.Quantity * ItemDefinition->LootValue;
			const FIntPoint Footprint = ItemDefinition->GetEffectiveGridSize(A.bRotated);
			OccupiedCells += Footprint.X * Footprint.Y;
			TestTrue(FString::Printf(TEXT("Entry remains inside 5x8 grid %d"), Index),
				FInventoryGridLibrary::IsInBounds(ContainerDefinition->GridSize, A.GridPosition,
					Footprint));
		}
	}
	TestTrue(TEXT("Generation reaches the value lower bound"), TotalValue >= ContainerDefinition->BaseTargetValue);
	TestTrue(TEXT("Generation deliberately leaves reserved cells"),
		ContainerDefinition->GridSize.X * ContainerDefinition->GridSize.Y - OccupiedCells >= ContainerDefinition->ReservedEmptyCells);

	FLootTableRow InvalidRow;
	InvalidRow.LootPoolId = TEXT("InvalidPool");
	InvalidRow.ItemDefinitionId = FPrimaryAssetId(TEXT("Item"), TEXT("MissingDefinition"));
	InvalidRow.Weight = 1.0f;
	Table->AddRow(TEXT("Invalid"), InvalidRow);
	ContainerDefinition->LootPoolId = TEXT("InvalidPool");
	TArray<FItemInstance> InvalidOutput;
	TestTrue(TEXT("Invalid-only table still terminates successfully"),
		FLootGenerationLibrary::Generate(*ContainerDefinition, 123456, ContainerId, 1, 1.0f, InvalidOutput));
	TestEqual(TEXT("Invalid rows generate no entries"), InvalidOutput.Num(), 0);

	return true;
}

#endif
