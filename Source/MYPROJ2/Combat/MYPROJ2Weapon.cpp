#include "Combat/MYPROJ2Weapon.h"
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

	// M2 placeholder: use a simple box mesh until real weapon art lands.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		// Scale to roughly pistol proportions: X=barrel (long), Y/Z=grip.
		Mesh->SetRelativeScale3D(FVector(0.4f, 0.05f, 0.05f));
		// Offset: forward 40cm, right 15cm, down 30cm so it reads as held at
		// hip/thigh level rather than floating at chest/head height.
		Mesh->SetRelativeLocation(FVector(40.f, 15.f, -30.f));
	}
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
	if (Mesh)
	{
		// Muzzle = forward edge of the mesh bounds.
		const FBoxSphereBounds Bounds = Mesh->CalcBounds(Mesh->GetComponentTransform());
		return Bounds.Origin + Mesh->GetForwardVector() * Bounds.BoxExtent.Y;
	}
	return GetActorLocation();
}
