// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/InteractionComponent.h"
#include "Interaction/Interactable.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "Network/MYPROJ2NetworkSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInteractionComponent, InteractSequence);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Local focus only on the owning client.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		UpdateLocalFocus();
	}
}

void UInteractionComponent::UpdateLocalFocus()
{
	FHitResult Hit;
	AActor* NewTarget = FindFocusTarget(Hit);

	if (NewTarget != CurrentFocus.Target)
	{
		CurrentFocus.Target = NewTarget;
		if (NewTarget && NewTarget->Implements<UInteractable>())
		{
			const ACharacter* Char = Cast<ACharacter>(GetOwner());
			const IInteractable* Interactable = Cast<IInteractable>(NewTarget);
			if (Interactable)
			{
				CurrentFocus.Prompt = Interactable->GetInteractionPrompt(Char);
				CurrentFocus.bCanInteract = Interactable->IsAvailableForInteraction(Char);
			}
		}
		else
		{
			CurrentFocus.Prompt = FText::GetEmpty();
			CurrentFocus.bCanInteract = false;
		}
	}
}

AActor* UInteractionComponent::FindFocusTarget(FHitResult& OutHit) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return nullptr;
	}

	const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return nullptr;
	}

	// Hit under cursor using visibility channel; interactables are expected to block Visibility.
	if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, OutHit))
	{
		return nullptr;
	}

	AActor* HitActor = OutHit.GetActor();
	if (!HitActor || !HitActor->Implements<UInteractable>())
	{
		return nullptr;
	}

	// Local distance hint. The Server performs its own check; this is just a UX nicety.
	const float MaxDist = UMYPROJ2NetworkSettings::Get()->MaxInteractDistance;
	if (FVector::DistSquared(OwnerPawn->GetActorLocation(), HitActor->GetActorLocation()) > FMath::Square(MaxDist))
	{
		return nullptr;
	}

	return HitActor;
}

void UInteractionComponent::TryInteract()
{
	// Owner-only path.
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (!CurrentFocus.bCanInteract || !CurrentFocus.Target)
	{
		return;
	}

	InteractSequence++;
	ServerTryInteract(CurrentFocus.Target, InteractSequence);
}

void UInteractionComponent::ServerTryInteract_Implementation(AActor* Target, int32 Sequence)
{
	// Authority-only path.

	if (!Target)
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::InvalidTarget);
		return;
	}

	// Drop replays / out-of-order duplicates.
	if (Sequence <= LastConsumedSequence)
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::Duplicate);
		return;
	}
	LastConsumedSequence = Sequence;

	if (!Target->Implements<UInteractable>())
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::InvalidTarget);
		return;
	}

	ACharacter* RequestingChar = Cast<ACharacter>(GetOwner());
	if (!RequestingChar)
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::InvalidTarget);
		return;
	}

	// Server-side availability check.
	IInteractable* Interactable = Cast<IInteractable>(Target);
	if (!Interactable || !Interactable->IsAvailableForInteraction(RequestingChar))
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::Unavailable);
		return;
	}

	// Server-side distance check.
	const float MaxDist = UMYPROJ2NetworkSettings::Get()->MaxInteractDistance;
	const float DistSq = FVector::DistSquared(RequestingChar->GetActorLocation(), Target->GetActorLocation());
	if (DistSq > FMath::Square(MaxDist))
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::TooFar);
		return;
	}

	// Server-side LOS check (channel = Visibility; interactables must block it).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractLOS), false, RequestingChar);
	FHitResult LOSHit;
	const FVector Start = RequestingChar->GetActorLocation();
	const FVector End = Target->GetActorLocation();
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(LOSHit, Start, End, ECC_Visibility, Params);
	if (bBlocked && LOSHit.GetActor() != Target)
	{
		ClientInteractionRejected(Target, EInteractionRejectReason::Obstructed);
		return;
	}

	// All checks passed — dispatch exactly once.
	Interactable->ExecuteInteraction(RequestingChar, this);

	UE_LOG(LogMYPROJ2Interaction, Log, TEXT("Interaction executed: %s by %s (seq=%d)"),
		*Target->GetName(), *RequestingChar->GetName(), Sequence);
}

void UInteractionComponent::ClientInteractionRejected_Implementation(AActor* Target, EInteractionRejectReason Reason)
{
	UE_LOG(LogMYPROJ2Interaction, Verbose, TEXT("Interaction rejected: %s on %s (reason=%d)"),
		Target ? *Target->GetName() : TEXT("<null>"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("<null>"),
		static_cast<int32>(Reason));
	// UI hook point: surface the reason to the local HUD in M2+.
}
