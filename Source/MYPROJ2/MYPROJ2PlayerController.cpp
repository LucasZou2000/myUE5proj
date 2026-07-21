// Copyright Epic Games, Inc. All Rights Reserved.

#include "MYPROJ2PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryTypes.h"
#include "Loot/LootContainer.h"
#include "MYPROJ2CollisionChannels.h"
#include "Network/MYPROJ2NetworkSettings.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

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
	ACharacter* Character = Cast<ACharacter>(GetPawn());
	if (!Character || (Character->FindComponentByClass<UHealthComponent>() && Character->FindComponentByClass<UHealthComponent>()->IsDead()))
	{
		OutReason = EInventoryRejectReason::Dead;
		return false;
	}
	if (!Container || !Container->IsGenerated() || !Container->GetInventoryComponent())
	{
		return false;
	}
	const float MaxDistance = UMYPROJ2NetworkSettings::Get()->MaxInteractDistance;
	if (FVector::DistSquared(Character->GetActorLocation(), Container->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		OutReason = EInventoryRejectReason::NoSpace;
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LootTransferLOS), false, Character);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Character->GetActorLocation(), Container->GetActorLocation(),
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

void AMYPROJ2PlayerController::ClientOpenLootContainer_Implementation(ALootContainer* Container)
{
	// M4 has no finished UI. This notification is the stable client hook for WBP_LootContainer.
}

void AMYPROJ2PlayerController::ClientLootTransferRejected_Implementation(uint16 RequestId, EInventoryRejectReason Reason)
{
	UE_LOG(LogTemp, Verbose, TEXT("Loot transfer rejected (request=%u, reason=%d)."), RequestId, static_cast<int32>(Reason));
}
