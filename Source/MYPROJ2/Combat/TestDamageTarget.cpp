#include "Combat/TestDamageTarget.h"
#include "Combat/HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ATestDamageTarget::ATestDamageTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Query-only; no Pawn block. Blocks Visibility so hitscan finds it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldStatic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);

	// Use engine cube so no external content needed.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
}

void ATestDamageTarget::BeginPlay()
{
	Super::BeginPlay();

	// Initial visual state, then poll for health changes.
	// We avoid binding to OnRep_Health directly because HealthComponent does not
	// expose a multicast delegate; polling keeps this simple and test-only.
	ApplyVisualState();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HealthPollTimer,
			this,
			&ATestDamageTarget::OnHealthChanged,
			0.1f,
			/*bLoop=*/true);
	}
}

void ATestDamageTarget::OnHealthChanged()
{
	ApplyVisualState();
}

void ATestDamageTarget::ApplyVisualState()
{
	if (!Mesh || !HealthComponent)
	{
		return;
	}

	const float Health = HealthComponent->GetHealth();
	const float MaxHealth = HealthComponent->GetMaxHealth();

	FLinearColor Colour;
	if (Health <= 0.f)
	{
		Colour = FLinearColor::Red;
	}
	else if (MaxHealth > 0.f && Health / MaxHealth <= 0.5f)
	{
		Colour = FLinearColor::Yellow;
	}
	else
	{
		Colour = FLinearColor::Green;
	}

	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
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
		DynMat->SetVectorParameterValue(TEXT("BaseColor"), Colour);
		DynMat->SetVectorParameterValue(TEXT("EmissiveColor"), Colour * 0.3f);
	}
}
