// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/MYPROJ2CharacterBase.h"
#include "Interaction/InteractionComponent.h"
#include "Combat/CombatComponent.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "Network/MYPROJ2NetworkSettings.h"
#include "MYPROJ2CollisionChannels.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"

AMYPROJ2CharacterBase::AMYPROJ2CharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Replicate so simulated proxies can follow. Server-authoritative movement.
	bReplicates = true;
	SetReplicateMovement(true);

	// Capsule.
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	// M2/M3: capsule blocks Visibility and the dedicated WeaponTrace channel so
	// other players' hitscan can hit us. (Default pawn capsule ignores Visibility
	// for the template's mouse-picking scenario; we explicitly enable both here
	// for combat.)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(MYPROJ2_TRACE_CHANNEL_WEAPON, ECR_Block);

	// Rotation model: capsule yaw is driven by ServerAimYaw (server-authoritative),
	// not by movement. Visual root yaws instantly on the owning client and smoothly
	// interpolates on simulated proxies.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = false; // M1: aim overrides facing.
	MoveComp->RotationRate = FRotator(0.f, 640.f, 0.f);
	MoveComp->bConstrainToPlane = true;
	MoveComp->bSnapToPlaneAtStart = true;

	// Camera boom.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	// Visual root that owns the mesh. Yaw-driven locally / via ServerAimYaw on proxies.
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);
	// Detach the skeletal mesh from the capsule and re-attach to VisualRoot so we can
	// rotate visuals independently of the capsule.
	GetMesh()->SetupAttachment(VisualRoot);

	// M1 placeholder visual: engine's built-in skeletal cube so the pawn is visible
	// in PIE without requiring character art. Will be replaced when M3+ brings in a
	// real character skeletal mesh.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlaceholderMesh(
		TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
	if (PlaceholderMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMesh(PlaceholderMesh.Object);
		// SkeletalCube's actual reference bounds are about 25.2cm high with its
		// origin at the bottom (verified through SkeletalMeshTools). Scale it to a
		// roughly 180cm tall, 63cm wide placeholder and align its feet with the
		// bottom of the 192cm capsule. The old 1.92 Z scale produced a 48cm pawn,
		// which made the correctly waist-height weapon look like it floated overhead.
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
		GetMesh()->SetRelativeScale3D(FVector(2.5f, 2.5f, 7.15f));
	}

	// Interaction driver.
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));

	// M2: combat + health.
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void AMYPROJ2CharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void AMYPROJ2CharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMYPROJ2CharacterBase, ServerAimYaw);
}

void AMYPROJ2CharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Enhanced Input bindings; the IMC itself is added by the controller.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMYPROJ2CharacterBase::OnMoveInput);
		}
		if (InteractAction)
		{
			EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AMYPROJ2CharacterBase::OnInteractInput);
		}
		if (FireAction)
		{
			EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AMYPROJ2CharacterBase::OnFireInput);
		}
		if (InventoryAction)
		{
			EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &AMYPROJ2CharacterBase::OnInventoryInput);
		}
	}
}

void AMYPROJ2CharacterBase::OnMoveInput(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Axis.IsNearlyZero())
	{
		// Top-down: world X = forward, world Y = right. WASD mapped directly.
		AddMovementInput(FVector::ForwardVector, Axis.Y);
		AddMovementInput(FVector::RightVector, Axis.X);
	}
}

void AMYPROJ2CharacterBase::OnInteractInput(const FInputActionValue& Value)
{
	if (InteractionComponent)
	{
		InteractionComponent->TryInteract();
	}
}

void AMYPROJ2CharacterBase::OnFireInput(const FInputActionValue& Value)
{
	if (CombatComponent)
	{
		CombatComponent->TryFire();
	}
}

void AMYPROJ2CharacterBase::OnInventoryInput(const FInputActionValue& Value)
{
	ToggleInventoryUI();
}

void AMYPROJ2CharacterBase::ToggleInventoryUI()
{
	// Only the owning client shows UI.
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Lazy-create the widget on first toggle.
	if (!InventoryWidgetInstance)
	{
		// Load the widget class from the asset registry. The path is stable
		// because M3 places WBP_InventoryGrid under /Game/Raid/UI/.
		const TSoftClassPtr<UUserWidget> WidgetClassPtr(FSoftObjectPath(TEXT("/Game/Raid/UI/WBP_InventoryGrid.WBP_InventoryGrid_C")));
		UClass* WidgetClass = WidgetClassPtr.LoadSynchronous();
		if (!WidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("MYPROJ2: WBP_InventoryGrid class not found at /Game/Raid/UI/WBP_InventoryGrid"));
			return;
		}
		InventoryWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
		if (!InventoryWidgetInstance)
		{
			return;
		}
	}

	// Toggle visibility.
	if (InventoryWidgetInstance->IsInViewport())
	{
		InventoryWidgetInstance->RemoveFromParent();
	}
	else
	{
		InventoryWidgetInstance->AddToViewport(0);
	}
}

bool AMYPROJ2CharacterBase::SampleCursorWorldLocation(FVector& OutWorldLocation) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	// Project cursor onto the gameplay plane (Z = pawn's feet) for stable aim regardless of floor geometry.
	const FVector PawnLocation = GetActorLocation();
	const FPlane AimPlane(PawnLocation, FVector::UpVector);

	FVector WorldOrigin, WorldDirection;
	if (!PC->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	const float Denom = FVector::DotProduct(WorldDirection, FVector::UpVector);
	if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float T = FVector::DotProduct(PawnLocation - WorldOrigin, FVector::UpVector) / Denom;
	if (T <= 0.f)
	{
		return false;
	}

	OutWorldLocation = WorldOrigin + WorldDirection * T;
	return true;
}

void AMYPROJ2CharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		TickLocalAim(DeltaSeconds);
	}
	else
	{
		TickSimulatedAim(DeltaSeconds);
	}

	// Server: rotate capsule toward ServerAimYaw.
	// Owning client keeps capsule un-rotated (visual root carries the yaw).
	// Simulated proxies see capsule rotation through standard movement replication.
	if (HasAuthority())
	{
		const FRotator CurrentRot = GetActorRotation();
		const FRotator TargetRot(0.f, ServerAimYaw, 0.f);
		if (!CurrentRot.Equals(TargetRot, 0.1f))
		{
			SetActorRotation(TargetRot);
		}
	}
}

void AMYPROJ2CharacterBase::TickLocalAim(float DeltaSeconds)
{
	// Sample cursor.
	bLocalCursorValid = SampleCursorWorldLocation(LocalAimTarget);
	if (!bLocalCursorValid || !VisualRoot)
	{
		return;
	}

	// Local visual root yaws instantly toward cursor.
	const FVector ToTarget = LocalAimTarget - GetActorLocation();
	if (ToTarget.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		const float LocalAimYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		VisualRoot->SetWorldRotation(FRotator(0.f, LocalAimYaw, 0.f));

		// Rate-limited aim upload.
		TimeSinceLastAimSend += DeltaSeconds;
		const float SendInterval = 1.0f / FMath::Max(1.0f, UMYPROJ2NetworkSettings::Get()->AimSendRateHz);
		const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(LastSentAimYaw, LocalAimYaw));
		if (TimeSinceLastAimSend >= SendInterval && YawDelta > 0.1f)
		{
			LastSentAimYaw = LocalAimYaw;
			TimeSinceLastAimSend = 0.0f;
			ServerUpdateAim(LocalAimYaw);
		}
	}
}

void AMYPROJ2CharacterBase::TickSimulatedAim(float DeltaSeconds)
{
	if (!VisualRoot)
	{
		return;
	}

	// Smoothly interpolate the visual root toward the replicated ServerAimYaw.
	const FRotator Current = VisualRoot->GetComponentRotation();
	const FRotator Target(0.f, ServerAimYaw, 0.f);
	const FRotator NewRot = FMath::RInterpTo(Current, Target, DeltaSeconds, 15.f);
	VisualRoot->SetWorldRotation(NewRot);
}

void AMYPROJ2CharacterBase::ServerUpdateAim_Implementation(float NewAimYaw)
{
	// Server: store and replicate. Capsule rotation happens in Tick.
	ServerAimYaw = FRotator::NormalizeAxis(NewAimYaw);
}
