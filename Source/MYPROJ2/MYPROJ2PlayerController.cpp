// Copyright Epic Games, Inc. All Rights Reserved.

#include "MYPROJ2PlayerController.h"
#include "MYPROJ2.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Character/MYPROJ2CharacterBase.h"
#include "Combat/HealthComponent.h"
#include "Combat/CombatComponent.h"
#include "Weapons/WeaponPartDefinition.h"
#include "Weapons/WeaponBuildTypes.h"
#include "Items/ItemDefinition.h"
#include "Base/BaseStashWidget.h"
#include "Framework/MYPROJ2PlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Loot/LootContainer.h"
#include "MYPROJ2CollisionChannels.h"
#include "Network/MYPROJ2NetworkSettings.h"
#include "Persistence/ProfileSubsystem.h"
#include "Persistence/ProfileSaveTypes.h"
#include "MYPROJ2GameMode.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AMYPROJ2PlayerController::AMYPROJ2PlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	// M3: raid inventory lives on the controller so it survives pawn respawn
	// and is private to Server + owning client.
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
}

void AMYPROJ2PlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AMYPROJ2PlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMYPROJ2PlayerController, CarriedCurrency);
}

void AMYPROJ2PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Add the mapping context once for the local player. Action bindings themselves
	// live on the possessed Character (AMYPROJ2CharacterBase).
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

bool AMYPROJ2PlayerController::IsLootRequestStale(uint16 RequestId) const
{
	if (!bHasConsumedLootRequest)
	{
		return false;
	}
	const int32 Delta = static_cast<int32>(RequestId) - static_cast<int32>(LastConsumedLootRequestId);
	const int32 WrappedDelta = ((Delta + 32768) % 65536 + 65536) % 65536 - 32768;
	return WrappedDelta <= 0;
}

void AMYPROJ2PlayerController::MarkLootRequestConsumed(uint16 RequestId)
{
	LastConsumedLootRequestId = RequestId;
	bHasConsumedLootRequest = true;
}

bool AMYPROJ2PlayerController::ValidateLootContainerRequest(ALootContainer* Container, EInventoryRejectReason& OutReason) const
{
	OutReason = EInventoryRejectReason::InvalidRequest;
	ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
	if (!ControlledCharacter || (ControlledCharacter->FindComponentByClass<UHealthComponent>() && ControlledCharacter->FindComponentByClass<UHealthComponent>()->IsDead()))
	{
		OutReason = EInventoryRejectReason::Dead;
		return false;
	}
	if (!Container || !Container->IsGenerated() || !Container->GetInventoryComponent())
	{
		return false;
	}
	const float MaxDistance = UMYPROJ2NetworkSettings::Get()->MaxInteractDistance;
	if (FVector::DistSquared(ControlledCharacter->GetActorLocation(), Container->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		OutReason = EInventoryRejectReason::NoSpace;
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LootTransferLOS), false, ControlledCharacter);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ControlledCharacter->GetActorLocation(), Container->GetActorLocation(),
		MYPROJ2_TRACE_CHANNEL_INTERACTION, Params) && Hit.GetActor() != Container)
	{
		OutReason = EInventoryRejectReason::InvalidRequest;
		return false;
	}
	OutReason = EInventoryRejectReason::None;
	return true;
}

void AMYPROJ2PlayerController::ServerTakeFromContainer_Implementation(ALootContainer* Container, FGuid ItemId, int32 Quantity, uint16 RequestId)
{
	if (IsLootRequestStale(RequestId))
	{
		ClientLootTransferRejected(RequestId, EInventoryRejectReason::StaleRequest);
		return;
	}
	EInventoryRejectReason Reason;
	if (!ValidateLootContainerRequest(Container, Reason) ||
		!Container->GetInventoryComponent()->AuthorityTransferTo(InventoryComponent, ItemId, Quantity, TOptional<FIntPoint>(), false, Reason))
	{
		ClientLootTransferRejected(RequestId, Reason);
		return;
	}
	MarkLootRequestConsumed(RequestId);
	Container->ForceNetUpdate();
}

void AMYPROJ2PlayerController::ServerPutIntoContainer_Implementation(ALootContainer* Container, FGuid ItemId, int32 Quantity,
	FIntPoint TargetPosition, bool bRotated, uint16 RequestId)
{
	if (IsLootRequestStale(RequestId))
	{
		ClientLootTransferRejected(RequestId, EInventoryRejectReason::StaleRequest);
		return;
	}
	EInventoryRejectReason Reason;
	if (!ValidateLootContainerRequest(Container, Reason) ||
		!InventoryComponent->AuthorityTransferTo(Container->GetInventoryComponent(), ItemId, Quantity, TargetPosition, bRotated, Reason))
	{
		ClientLootTransferRejected(RequestId, Reason);
		return;
	}
	MarkLootRequestConsumed(RequestId);
	Container->ForceNetUpdate();
}

void AMYPROJ2PlayerController::BeginLootOpenDelay(ALootContainer* Container)
{
	if (!Container)
	{
		return;
	}
	PendingLootContainer = Container;
	bLootOpenAcknowledged = false;
	GetWorldTimerManager().ClearTimer(LootOpenDelayTimer);
	GetWorldTimerManager().SetTimer(LootOpenDelayTimer, this,
		&AMYPROJ2PlayerController::CompleteLootOpenDelay, 0.2f, false);
}

void AMYPROJ2PlayerController::ClientOpenLootContainer_Implementation(ALootContainer* Container)
{
	if (Container && PendingLootContainer.Get() == Container)
	{
		// The Server invokes this only after it has generated durable contents.
		bLootOpenAcknowledged = true;
	}
}

void AMYPROJ2PlayerController::CompleteLootOpenDelay()
{
	ALootContainer* Container = PendingLootContainer.Get();
	// The Server sends ClientOpenLootContainer only after AuthorityEnsureGenerated succeeds.
	// The player inventory UI does not depend on the container Fast Array arriving first.
	const bool bCanShow = bLootOpenAcknowledged && Container;
	PendingLootContainer.Reset();
	bLootOpenAcknowledged = false;
	if (bCanShow)
	{
		ShowLootContainer(Container);
	}
}

void AMYPROJ2PlayerController::ShowLootContainer(ALootContainer* Container)
{
	if (!Container || !IsLocalPlayerController())
	{
		return;
	}
	if (AMYPROJ2CharacterBase* RaidCharacter = Cast<AMYPROJ2CharacterBase>(GetPawn()))
	{
		RaidCharacter->ShowInventoryUI();
	}
}

void AMYPROJ2PlayerController::ClientLootTransferRejected_Implementation(uint16 RequestId, EInventoryRejectReason Reason)
{
	UE_LOG(LogTemp, Verbose, TEXT("Loot transfer rejected (request=%u, reason=%d)."), RequestId, static_cast<int32>(Reason));
}

void AMYPROJ2PlayerController::AuthoritySetCarriedCurrency(int64 NewCurrency)
{
	if (HasAuthority())
	{
		CarriedCurrency = FMath::Max<int64>(0, NewCurrency);
	}
}

void AMYPROJ2PlayerController::ServerRequestExtraction_Implementation()
{
	if (AMYPROJ2GameMode* RaidGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMYPROJ2GameMode>() : nullptr)
	{
		RaidGameMode->AuthorityExtractPlayer(this);
	}
}

void AMYPROJ2PlayerController::ClientFinalizeRaidSettlement_Implementation(const FRaidSettlementPayload& Settlement)
{
	if (UProfileSubsystem* ProfileSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProfileSubsystem>() : nullptr)
	{
		ProfileSubsystem->ApplyRaidSettlement(Settlement);
	}
	OpenBaseStashUI();
}

void AMYPROJ2PlayerController::OpenBaseStashUI()
{
	if (!IsLocalPlayerController())
	{
		UE_LOG(LogMYPROJ2, Warning, TEXT("OpenBaseStashUI ignored on a non-local controller."));
		return;
	}
	if (!BaseStashWidget)
	{
		BaseStashWidget = CreateWidget<UBaseStashWidget>(this, UBaseStashWidget::StaticClass());
		UE_LOG(LogMYPROJ2, Log, TEXT("Base stash widget created: %s"), *GetNameSafe(BaseStashWidget));
	}
	if (BaseStashWidget)
	{
		BaseStashWidget->InitializeForController(this);
		BaseStashWidget->SetVisibility(ESlateVisibility::Visible);
		if (!BaseStashWidget->IsInViewport())
		{
			BaseStashWidget->AddToViewport(30);
			UE_LOG(LogMYPROJ2, Log, TEXT("Base stash widget added to viewport."));
		}
	}
	else
	{
		UE_LOG(LogMYPROJ2, Error, TEXT("Failed to create base stash widget."));
	}
}

void AMYPROJ2PlayerController::OpenBaseStash()
{
	OpenBaseStashUI();
}

void AMYPROJ2PlayerController::DebugExtract()
{
	if (IsLocalPlayerController())
	{
		ServerRequestExtraction();
	}
}

void AMYPROJ2PlayerController::DebugKillRaid()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	if (AMYPROJ2CharacterBase* RaidCharacter = Cast<AMYPROJ2CharacterBase>(GetPawn()))
	{
		if (UHealthComponent* Health = RaidCharacter->GetHealthComponent())
		{
			// Health applies damage only on the Server; this command is for Standalone PIE acceptance.
			Health->ApplyDamage(Health->GetMaxHealth());
		}
	}
}

void AMYPROJ2PlayerController::DebugGrantStashCurrency(int64 Amount)
{
	if (IsLocalPlayerController())
	{
		if (UProfileSubsystem* ProfileSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProfileSubsystem>() : nullptr)
		{
			ProfileSubsystem->DebugGrantStashCurrency(Amount);
		}
	}
}

void AMYPROJ2PlayerController::DebugInstallWeaponPart(int32 Slot)
{
	if (!IsLocalPlayerController() || Slot < 0 || Slot > static_cast<int32>(EWeaponPartSlot::Optic))
	{
		return;
	}
	ServerDebugInstallWeaponPart(Slot);
}

void AMYPROJ2PlayerController::ServerDebugInstallWeaponPart_Implementation(int32 Slot)
{
	if (!HasAuthority() || Slot < 0 || Slot > static_cast<int32>(EWeaponPartSlot::Optic))
	{
		return;
	}
	AMYPROJ2CharacterBase* RaidCharacter = Cast<AMYPROJ2CharacterBase>(GetPawn());
	UCombatComponent* Combat = RaidCharacter ? RaidCharacter->FindComponentByClass<UCombatComponent>() : nullptr;
	if (!Combat || !InventoryComponent)
	{
		UE_LOG(LogMYPROJ2Combat, Warning, TEXT("DebugInstallWeaponPart: missing server combat or inventory component"));
		return;
	}
	const EWeaponPartSlot RequestedSlot = static_cast<EWeaponPartSlot>(Slot);
	for (const FReplicatedInventoryEntry& Entry : InventoryComponent->GetEntries())
	{
		if (const UWeaponPartDefinition* Part = Cast<UWeaponPartDefinition>(InventoryComponent->ResolveDefinition(Entry.Item.DefinitionId));
			Part && Part->Slot == RequestedSlot && Entry.Item.Quantity == 1)
		{
			UE_LOG(LogMYPROJ2Combat, Log, TEXT("DebugInstallWeaponPart: server selected %s (%s)"),
				*Entry.Item.InstanceId.ToString(), *Entry.Item.DefinitionId.ToString());
			Combat->ServerInstallPart_Implementation(Entry.Item.InstanceId, RequestedSlot, Combat->AllocateAssemblyRequestId());
			return;
		}
	}
	UE_LOG(LogMYPROJ2Combat, Warning, TEXT("DebugInstallWeaponPart: server inventory has no matching part for slot %d"), Slot);
}

void AMYPROJ2PlayerController::DebugRemoveWeaponPart(int32 Slot)
{
	if (!IsLocalPlayerController() || Slot < 0 || Slot > static_cast<int32>(EWeaponPartSlot::Optic))
	{
		return;
	}
	ServerDebugRemoveWeaponPart(Slot);
}

void AMYPROJ2PlayerController::ServerDebugRemoveWeaponPart_Implementation(int32 Slot)
{
	if (!HasAuthority() || Slot < 0 || Slot > static_cast<int32>(EWeaponPartSlot::Optic))
	{
		return;
	}
	if (AMYPROJ2CharacterBase* RaidCharacter = Cast<AMYPROJ2CharacterBase>(GetPawn()))
	{
		if (UCombatComponent* Combat = RaidCharacter->FindComponentByClass<UCombatComponent>())
		{
			Combat->ServerRemovePart_Implementation(static_cast<EWeaponPartSlot>(Slot), Combat->AllocateAssemblyRequestId());
		}
	}
}

void AMYPROJ2PlayerController::DebugWeaponStats()
{
	if (AMYPROJ2CharacterBase* RaidCharacter = Cast<AMYPROJ2CharacterBase>(GetPawn()))
	{
		if (const UCombatComponent* Combat = RaidCharacter->FindComponentByClass<UCombatComponent>())
		{
			const FWeaponStatBlock& Stats = Combat->GetDerivedStats();
			UE_LOG(LogMYPROJ2Combat, Log, TEXT("M6 weapon stats: damage=%.2f interval=%.3f range=%.0f spread=%.2f noise=%.0f mag=%d parts=%d"),
				Stats.Damage, Stats.FireIntervalSeconds, Stats.RangeCm, Stats.SpreadDegrees,
				Stats.NoiseRadiusCm, Stats.MagazineSize, Combat->GetInstalledParts().Num());
		}
	}
}

void AMYPROJ2PlayerController::DebugWeaponInventory()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	UE_LOG(LogMYPROJ2Combat, Log, TEXT("M6 inventory entries: %d"), InventoryComponent ? InventoryComponent->GetEntries().Num() : 0);
	if (InventoryComponent)
	{
		for (const FReplicatedInventoryEntry& Entry : InventoryComponent->GetEntries())
		{
			UE_LOG(LogMYPROJ2Combat, Log, TEXT("M6 inventory item: instance=%s definition=%s quantity=%d position=(%d,%d)"),
				*Entry.Item.InstanceId.ToString(), *Entry.Item.DefinitionId.ToString(), Entry.Item.Quantity,
				Entry.Item.GridPosition.X, Entry.Item.GridPosition.Y);
		}
	}
}

void AMYPROJ2PlayerController::DebugGrantWeaponTestParts()
{
	if (IsLocalPlayerController())
	{
		ServerDebugGrantWeaponTestParts();
	}
}

void AMYPROJ2PlayerController::ServerDebugGrantWeaponTestParts_Implementation()
{
	if (!HasAuthority() || !InventoryComponent)
	{
		return;
	}
	static const TCHAR* TestPartPaths[] =
	{
		TEXT("/Game/Raid/Data/Items/DA_Part_TestBarrel.DA_Part_TestBarrel"),
		TEXT("/Game/Raid/Data/Items/DA_Part_TestStock.DA_Part_TestStock")
	};
	for (const TCHAR* Path : TestPartPaths)
	{
		if (UItemDefinition* Definition = LoadObject<UItemDefinition>(nullptr, Path))
		{
			const bool bAdded = InventoryComponent->AuthorityTryAdd(Definition, 1);
			UE_LOG(LogMYPROJ2Combat, Log, TEXT("DebugGrantWeaponTestParts: %s -> %s"),
				*Definition->GetPrimaryAssetId().ToString(), bAdded ? TEXT("added") : TEXT("inventory full/failed"));
		}
		else
		{
			UE_LOG(LogMYPROJ2Combat, Error, TEXT("DebugGrantWeaponTestParts: failed to load %s"), Path);
		}
	}
}
