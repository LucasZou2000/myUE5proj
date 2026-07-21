// Copyright Epic Games, Inc. All Rights Reserved.

#include "MYPROJ2PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Inventory/InventoryComponent.h"

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
