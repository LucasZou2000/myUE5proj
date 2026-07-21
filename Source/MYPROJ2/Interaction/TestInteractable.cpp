// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/TestInteractable.h"
#include "Interaction/InteractionComponent.h"
#include "MYPROJ2CollisionChannels.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ATestInteractable::ATestInteractable()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;

	// M3: block the dedicated InteractionTrace channel so the focus and LOS
	// checks find this actor. Visibility blocking is retained for legacy aim
	// code paths that still sample it (e.g. camera-based picking helpers).
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCollisionResponseToChannel(MYPROJ2_TRACE_CHANNEL_INTERACTION, ECR_Block);

	// Use the built-in cube so the test map does not require external content.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetWorldScale3D(FVector(0.5f));
}

void ATestInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATestInteractable, bActivated);
}

bool ATestInteractable::IsAvailableForInteraction(const ACharacter* RequestingCharacter) const
{
	// Always available; toggles back and forth.
	return true;
}

FText ATestInteractable::GetInteractionPrompt(const ACharacter* RequestingCharacter) const
{
	return bActivated
		? NSLOCTEXT("MYPROJ2", "TestInteractableOff", "Deactivate")
		: NSLOCTEXT("MYPROJ2", "TestInteractableOn", "Activate");
}

void ATestInteractable::ExecuteInteraction(ACharacter* RequestingCharacter, UInteractionComponent* RequestingComponent)
{
	// Authority-only — InteractionComponent only calls this on Server.
	bActivated = !bActivated;

	// Mirror OnRep locally so the host's view updates identically to clients.
	OnRep_Activated();
}

void ATestInteractable::OnRep_Activated()
{
	ApplyVisualState();
}

void ATestInteractable::ApplyVisualState()
{
	if (!Mesh)
	{
		return;
	}

	// The default /Engine/BasicShapes/BasicShapeMaterial does not expose a usable
	// colour parameter, so SetVectorParameterValue("Color") silently no-ops.
	// Reliable path: create a dynamic material instance of
	// /Engine/EngineMaterials/DefaultMaterial (which has a BaseColor parameter)
	// and set it once per toggle. We cache the MID on the actor so we don't
	// allocate a new one every flip.
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	if (!Parent)
	{
		// Fallback to basic shape material; some engine builds ship without
		// DefaultMaterial. In that case at least CustomPrimitiveData will work
		// if the base material reads it.
		Parent = Mesh->GetMaterial(0);
	}

	if (!Parent)
	{
		return;
	}

	UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!DynMat || DynMat->Parent != Parent)
	{
		DynMat = UMaterialInstanceDynamic::Create(Parent, this);
		Mesh->SetMaterial(0, DynMat);
	}

	if (DynMat)
	{
		const FLinearColor Colour = bActivated ? FLinearColor::Green : FLinearColor::Red;
		// DefaultMaterial exposes "BaseColor" and "EmissiveColor".
		DynMat->SetVectorParameterValue(TEXT("BaseColor"), Colour);
		DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), Colour * 0.5f);
	}
}
