#include "Combat/CombatComponent.h"
#include "Combat/WeaponData.h"
#include "Combat/MYPROJ2Weapon.h"
#include "Combat/HealthComponent.h"
#include "Character/MYPROJ2CharacterBase.h"
#include "Network/MYPROJ2NetworkSettings.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && WeaponData)
	{
		CurrentAmmo = WeaponData->Ammo;
		EnsureWeaponSpawned();
	}
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Per m2-combat.md: CurrentAmmo is owner-only.
	DOREPLIFETIME_CONDITION(UCombatComponent, CurrentAmmo, COND_OwnerOnly);
}

void UCombatComponent::EnsureWeaponSpawned()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !WeaponData || !WeaponData->WeaponClass)
	{
		return;
	}

	if (Weapon)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Weapon = GetWorld()->SpawnActor<AMYPROJ2Weapon>(WeaponData->WeaponClass, Params);
	if (Weapon)
	{
		Weapon->AttachToOwner();
		UE_LOG(LogMYPROJ2Combat, Log, TEXT("%s spawned weapon %s"),
			*GetNameSafe(Owner), *GetNameSafe(Weapon));
	}
}

void UCombatComponent::TryFire()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Owning client builds the StartInfo from its local view and submits.
	// Per m2-combat.md, the fire direction must match what the player actually
	// sees — in our M1 top-down rig the camera is fixed, so we derive the
	// direction from the cursor-hit-point on the ground plane, same as M1 aim.
	APlayerController* PC = Cast<APlayerController>(Owner->GetInstigatorController());
	if (!PC)
	{
		return;
	}

	// Cursor target on the ground plane (same channel M1 uses for aim focus).
	FHitResult CursorHit;
	const bool bHasCursorTarget = PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);

	// FireOrigin: actor origin (capsule centre, waist height). No vertical offset —
	// visual offsets are presentation concerns that belong on the weapon mesh,
	// not on the authoritative trace origin.
	const FVector FireOrigin = Owner->GetActorLocation();

	// FireDirection: horizontal vector toward the cursor target. If the cursor
	// is not over anything traceable, fall back to the actor's current facing.
	FVector FireDirection;
	if (bHasCursorTarget)
	{
		FVector ToTarget = CursorHit.ImpactPoint - FireOrigin;
		ToTarget.Z = 0.f; // M2 hitscan is horizontal; vertical aim is a M3+ concern.
		FireDirection = ToTarget.GetSafeNormal();
		if (FireDirection.IsNearlyZero())
		{
			FireDirection = Owner->GetActorForwardVector();
		}
	}
	else
	{
		FireDirection = Owner->GetActorForwardVector();
	}

	FFireStartInfo StartInfo;
	StartInfo.FireOrigin = FireOrigin;
	StartInfo.FireDirection = FireDirection;
	// M2: use FireOrigin as the muzzle — the placeholder weapon mesh has no
	// hand_r socket on the skeletal cube, so GetMuzzleLocation() returns an
	// unreliable point tied to the bone root orientation. Real muzzle sockets
	// arrive with real weapon art in M3+.
	StartInfo.MuzzleSocket = FireOrigin;

	ServerTryFire(StartInfo);
}

bool UCombatComponent::ValidateFireRequest(
	const FFireStartInfo& StartInfo,
	EFireRejectReason& OutReason,
	FVector& OutExpectedOrigin) const
{
	const UMYPROJ2NetworkSettings* Settings = GetDefault<UMYPROJ2NetworkSettings>();
	AActor* Owner = GetOwner();

	// 1. Weapon + data.
	if (!WeaponData || !Weapon)
	{
		OutReason = EFireRejectReason::NoWeapon;
		return false;
	}

	// 2. Ammo.
	if (CurrentAmmo <= 0)
	{
		OutReason = EFireRejectReason::NoAmmo;
		return false;
	}

	// 3. Rate limit: shots-per-minute -> seconds between shots.
	const float SecondsBetweenShots = 60.f / FMath::Max(1.f, WeaponData->FireRate);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (LastAcceptedFireTime > 0.f && (Now - LastAcceptedFireTime) < SecondsBetweenShots * 0.95f)
	{
		// 5% grace to absorb RPC transit time on the authoritative clock.
		OutReason = EFireRejectReason::RateLimited;
		return false;
	}

	// 4. Fire direction must be a unit-ish vector.
	const float DirLenSq = StartInfo.FireDirection.SizeSquared();
	if (DirLenSq < 0.81f || DirLenSq > 1.21f) // 0.9^2 .. 1.1^2
	{
		OutReason = EFireRejectReason::InvalidFireDirection;
		return false;
	}

	// 5. Origin tolerance: expected = actor chest position on Server, matching
	// the client-side TryFire construction. We do NOT use the camera location
	// because M1's camera is a fixed top-down rig, far from the character.
	{
		const FVector ServerOrigin = Owner->GetActorLocation();
		OutExpectedOrigin = ServerOrigin;

		const float DistSq = FVector::DistSquared(StartInfo.FireOrigin, ServerOrigin);
		const float Tolerance = Settings->MaxFireOriginError;
		if (DistSq > Tolerance * Tolerance)
		{
			OutReason = EFireRejectReason::OriginMismatch;
			return false;
		}
	}

	OutReason = EFireRejectReason::None;
	return true;
}

void UCombatComponent::ExecuteFire(const FFireStartInfo& StartInfo)
{
	AActor* Owner = GetOwner();
	if (!Owner || !WeaponData)
	{
		return;
	}

	// Decrement ammo (replicates to owner).
	CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
	LastAcceptedFireTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Hitscan from the authoritative actor origin (matches client TryFire).
	const FVector TraceStart = Owner->GetActorLocation();
	const FVector TraceEnd = TraceStart + StartInfo.FireDirection * WeaponData->Range;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MYPROJ2FireTrace), /*bTraceComplex=*/false, Owner);
	QueryParams.AddIgnoredActor(Owner);

	FHitResult Hit;
	// M2 uses ECC_Visibility as the hitscan channel. A custom WeaponTrace channel
	// would require ProjectSettings->Collision wiring which is Editor-side; using
	// Visibility keeps M2 self-contained. Both TestDamageTarget and player meshes
	// default to blocking Visibility.
	const ECollisionChannel FireChannel = ECC_Visibility;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, FireChannel, QueryParams);

	// Apply damage on hit (server-authoritative).
	if (bHit && Hit.GetActor())
	{
		if (UHealthComponent* Health = Hit.GetActor()->FindComponentByClass<UHealthComponent>())
		{
			Health->ApplyDamage(WeaponData->Damage);
			UE_LOG(LogMYPROJ2Combat, Log, TEXT("%s hit %s for %.1f damage"),
				*GetNameSafe(Owner), *GetNameSafe(Hit.GetActor()), WeaponData->Damage);
		}
	}

	// Build firing context for multicast feedback.
	FFiringContext Ctx;
	Ctx.FireOrigin = TraceStart;
	Ctx.FireDirection = StartInfo.FireDirection;
	Ctx.MuzzleSocket = StartInfo.MuzzleSocket;
	Ctx.FireRate = WeaponData->FireRate;
	Ctx.AmmoAfterShot = CurrentAmmo;

	MulticastFireFeedback(Ctx, Hit);
}

void UCombatComponent::ServerTryFire_Implementation(const FFireStartInfo& StartInfo)
{
	EFireRejectReason Reason = EFireRejectReason::None;
	FVector ExpectedOrigin = FVector::ZeroVector;

	if (!ValidateFireRequest(StartInfo, Reason, ExpectedOrigin))
	{
		FFireRejectionInfo Info;
		Info.Reason = Reason;
		Info.ServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Info.ExpectedOrigin = ExpectedOrigin;
		ClientFireRejected(Info);

		UE_LOG(LogMYPROJ2Combat, Log, TEXT("Fire rejected for %s: reason=%d"),
			*GetNameSafe(GetOwner()), static_cast<int32>(Reason));
		return;
	}

	ExecuteFire(StartInfo);
}

void UCombatComponent::ClientFireRejected_Implementation(const FFireRejectionInfo& RejectionInfo)
{
	// Owning client: surface to HUD/log. M2 has no HUD, so log only.
	UE_LOG(LogMYPROJ2Combat, Log, TEXT("Fire rejected (server): reason=%d expected_origin=%s"),
		static_cast<int32>(RejectionInfo.Reason),
		*RejectionInfo.ExpectedOrigin.ToCompactString());
}

void UCombatComponent::MulticastFireFeedback_Implementation(const FFiringContext& FiringContext, const FHitResult& ImpactResult)
{
	// M2 placeholder: log + debug line. Step 5 adds proper tracer/muzzle/impact hooks.
	if (UWorld* World = GetWorld())
	{
		FVector TraceEnd = FiringContext.FireOrigin + FiringContext.FireDirection * 5000.f;
		if (ImpactResult.bBlockingHit)
		{
			TraceEnd = FVector(ImpactResult.ImpactPoint);
		}

		DrawDebugLine(World, FiringContext.MuzzleSocket, TraceEnd, FColor::Yellow, false, 0.5f, 0, 1.5f);
		if (ImpactResult.bBlockingHit)
		{
			DrawDebugPoint(World, ImpactResult.ImpactPoint, 8.f, FColor::Red, false, 0.5f);
		}
	}

	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("%s fire feedback: ammo=%d hit=%s"),
		*GetNameSafe(GetOwner()),
		FiringContext.AmmoAfterShot,
		ImpactResult.bBlockingHit ? *GetNameSafe(ImpactResult.GetActor()) : TEXT("none"));
}

void UCombatComponent::OnRep_CurrentAmmo(int32 PreviousAmmo)
{
	// M2 placeholder for future HUD hook.
	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("%s ammo rep: %d -> %d"),
		*GetNameSafe(GetOwner()), PreviousAmmo, CurrentAmmo);
}
