// Copyright Epic Games, Inc. All Rights Reserved.

#include "MYPROJ2PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "MYPROJ2Character.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "MYPROJ2.h"

AMYPROJ2PlayerController::AMYPROJ2PlayerController()
{
	bIsTouch = false;
	bMoveToMouseCursor = false;

	// create the path following comp
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("Path Following Component"));

	// configure the controller
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	CachedDestination = FVector::ZeroVector;
	FollowTime = 0.f;
}

void AMYPROJ2PlayerController::SetupInputComponent()
{
	// set up gameplay key bindings
	Super::SetupInputComponent();

	// Only set up input on local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}

		// Set up action bindings
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
		{
			// Setup mouse input events
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &AMYPROJ2PlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &AMYPROJ2PlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &AMYPROJ2PlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &AMYPROJ2PlayerController::OnSetDestinationReleased);

			// Setup touch input events
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &AMYPROJ2PlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &AMYPROJ2PlayerController::OnTouchTriggered);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &AMYPROJ2PlayerController::OnTouchReleased);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &AMYPROJ2PlayerController::OnTouchReleased);
		}
		else
		{
			UE_LOG(LogMYPROJ2, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
		}
	}
}

void AMYPROJ2PlayerController::OnInputStarted()
{
	StopMovement();

	// Update the move destination to wherever the cursor is pointing at
	UpdateCachedDestination();
}

void AMYPROJ2PlayerController::OnSetDestinationTriggered()
{
	// We flag that the input is being pressed
	FollowTime += GetWorld()->GetDeltaSeconds();
	
	// Update the move destination to wherever the cursor is pointing at
	UpdateCachedDestination();
	
	// Move towards mouse pointer or touch
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn != nullptr)
	{
		FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
		ControlledPawn->AddMovementInput(WorldDirection, 1.0, false);
	}
}

void AMYPROJ2PlayerController::OnSetDestinationReleased()
{
	// If it was a short press
	if (FollowTime <= ShortPressThreshold)
	{
		// We move there and spawn some particles
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, CachedDestination);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FXCursor, CachedDestination, FRotator::ZeroRotator, FVector(1.f, 1.f, 1.f), true, true, ENCPoolMethod::None, true);
	}

	FollowTime = 0.f;
}

// Triggered every frame when the input is held down
void AMYPROJ2PlayerController::OnTouchTriggered()
{
	bIsTouch = true;
	OnSetDestinationTriggered();
}

void AMYPROJ2PlayerController::OnTouchReleased()
{
	bIsTouch = false;
	OnSetDestinationReleased();
}

void AMYPROJ2PlayerController::UpdateCachedDestination()
{
	// We look for the location in the world where the player has pressed the input
	FHitResult Hit;
	bool bHitSuccessful = false;
	if (bIsTouch)
	{
		bHitSuccessful = GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit);
	}
	else
	{
		bHitSuccessful = GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit);
	}

	// If we hit a surface, cache the location
	if (bHitSuccessful)
	{
		CachedDestination = Hit.Location;
	}
}
