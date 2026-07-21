#include "Loot/LootContainer.h"

#include "Components/StaticMeshComponent.h"
#include "Framework/MYPROJ2GameState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/InteractionComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Loot/LootContainerDefinition.h"
#include "Loot/LootGenerationLibrary.h"
#include "MYPROJ2PlayerController.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogMYPROJ2LootContainer, Log, All);

ALootContainer::ALootContainer()
{
	bReplicates = true;
	SetReplicateMovement(false);
	NetUpdateFrequency = 2.0f;
	ContainerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContainerMesh"));
	SetRootComponent(ContainerMesh);
	ContainerMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Inventory->GridColumns = 5;
	Inventory->GridRows = 8;
}

void ALootContainer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!ContainerId.IsValid())
	{
		ContainerId = FGuid::NewGuid();
	}
}

void ALootContainer::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !ContainerId.IsValid())
	{
		ContainerId = FGuid::NewGuid();
		UE_LOG(LogMYPROJ2LootContainer, Warning, TEXT("Container '%s' had no ContainerId; generated a runtime ID."), *GetName());
	}
}

void ALootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALootContainer, bGenerated);
	DOREPLIFETIME(ALootContainer, bHasBeenOpened);
}

bool ALootContainer::IsAvailableForInteraction(const ACharacter* RequestingCharacter) const
{
	return RequestingCharacter != nullptr && Definition != nullptr;
}

FText ALootContainer::GetInteractionPrompt(const ACharacter* RequestingCharacter) const
{
	return FText::FromString(bHasBeenOpened ? TEXT("Search container") : TEXT("Open container"));
}

void ALootContainer::ExecuteInteraction(ACharacter* RequestingCharacter, UInteractionComponent* RequestingComponent)
{
	if (!HasAuthority() || !RequestingCharacter || !AuthorityEnsureGenerated())
	{
		return;
	}
	bHasBeenOpened = true;
	ForceNetUpdate();
	if (AMYPROJ2PlayerController* Controller = Cast<AMYPROJ2PlayerController>(RequestingCharacter->GetController()))
	{
		Controller->ClientOpenLootContainer(this);
	}
}

bool ALootContainer::AuthorityEnsureGenerated()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (bGenerated)
	{
		return true;
	}
	if (!Definition || !Inventory)
	{
		UE_LOG(LogMYPROJ2LootContainer, Warning, TEXT("Container '%s' cannot generate without definition/inventory."), *GetName());
		return false;
	}
	AMYPROJ2GameState* GameState = GetWorld() ? GetWorld()->GetGameState<AMYPROJ2GameState>() : nullptr;
	if (!GameState)
	{
		return false;
	}

	Inventory->GridColumns = Definition->GridSize.X;
	Inventory->GridRows = Definition->GridSize.Y;
	TArray<FItemInstance> GeneratedItems;
	if (!FLootGenerationLibrary::Generate(*Definition, GameState->GetRaidSeed(), ContainerId,
		GenerationVersion, GameState->GetLootValueMultiplier(), GeneratedItems))
	{
		return false;
	}
	for (FItemInstance& Item : GeneratedItems)
	{
		FReplicatedInventoryEntry& Entry = Inventory->ReplicatedItems.Entries.AddDefaulted_GetRef();
		Entry.Item = MoveTemp(Item);
		Entry.OwnerComponent = Inventory;
		Inventory->ReplicatedItems.MarkItemDirty(Entry);
	}
	bGenerated = true;
	Inventory->OnInventoryChanged.Broadcast();
	ForceNetUpdate();
	return true;
}

void ALootContainer::OnRep_Generated()
{
	// Durable state only. UI observes inventory Fast Array and this flag.
}

void ALootContainer::OnRep_OpenState()
{
	// Presentation hook for a future open-lid mesh.
}
