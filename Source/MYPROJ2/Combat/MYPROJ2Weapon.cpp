#include "Combat/MYPROJ2Weapon.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AMYPROJ2Weapon::AMYPROJ2Weapon()
{
	PrimaryActorTick.bCanEverTick = false;

	// Replicate so all clients see the weapon on the owner.
	bReplicates = true;
	SetReplicatingMovement(false); // attached to character; no independent movement replication

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Root);

	// M2 placeholder: use a simple box mesh until real weapon art lands.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		// Placeholder rifle: local X is the barrel axis. Keep it at waist height
		// and slightly to the character's right until a real hand socket exists.
		Mesh->SetRelativeScale3D(FVector(0.55f, 0.06f, 0.06f));
		Mesh->SetRelativeLocation(FVector(35.f, 18.f, -25.f));
	}

	// Cube length is 55cm and centred at X=35, so its forward face is X=62.5.
	Muzzle->SetRelativeLocation(FVector(62.5f, 18.f, -25.f));
}

void AMYPROJ2Weapon::AttachToOwner()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// M2 placeholder rig: attach to the character's VisualRoot so the weapon
	// follows cursor-driven aim yaw (same as the skeletal mesh). The placeholder
	// SkeletalCube has no hand_r socket and its root bone orientation is
	// unreliable, which made the weapon appear to point skyward when attached
	// to the mesh. Attaching to VisualRoot gives a clean horizontal yaw that
	// tracks M1's ServerAimYaw replication. Real socket attach lands with
	// real character art in M3+.
	USceneComponent* AttachParent = OwnerActor->GetRootComponent();
	// MYPROJ2CharacterBase names its aim-pivot scene component "VisualRoot".
	if (UObject* Found = OwnerActor->GetDefaultSubobjectByName(TEXT("VisualRoot")))
	{
		if (USceneComponent* VisualRoot = Cast<USceneComponent>(Found))
		{
			AttachParent = VisualRoot;
		}
	}

	if (!AttachParent)
	{
		return;
	}

	AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeRotation(FRotator::ZeroRotator);
}

FVector AMYPROJ2Weapon::GetMuzzleLocation() const
{
	if (Muzzle)
	{
		return Muzzle->GetComponentLocation();
	}
	return GetActorLocation();
}
