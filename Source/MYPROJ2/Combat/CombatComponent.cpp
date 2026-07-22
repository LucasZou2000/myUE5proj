#include "Combat/CombatComponent.h"
#include "Combat/WeaponData.h"
#include "Combat/MYPROJ2Weapon.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Weapons/WeaponPartDefinition.h"
#include "Weapons/WeaponStatLibrary.h"
#include "Character/MYPROJ2CharacterBase.h"
#include "Network/MYPROJ2NetworkSettings.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "MYPROJ2CollisionChannels.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"

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
		DerivedStats = WeaponData->GetEffectiveBaseStats();
		CurrentAmmo = DerivedStats.MagazineSize;
		EnsureWeaponSpawned();
	}
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Per m2-combat.md: CurrentAmmo is owner-only.
	DOREPLIFETIME_CONDITION(UCombatComponent, CurrentAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UCombatComponent, Weapon);
	DOREPLIFETIME_CONDITION(UCombatComponent, InstalledParts, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCombatComponent, DerivedStats, COND_OwnerOnly);
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

	// Use the replicated weapon muzzle when available. The fallback matches the
	// placeholder weapon's relative muzzle closely enough during initial replication.
	const FVector MuzzleLocation = Weapon
		? Weapon->GetMuzzleLocation()
		: Owner->GetActorTransform().TransformPosition(FVector(62.5f, 18.f, -25.f));

	FHitResult CursorHit;
	// Cursor targeting for aim uses the WeaponTrace channel so interactables
	// (InteractionTrace-blockers) do not steal aim focus and vice versa.
	const bool bHasCursorTarget = PC->GetHitResultUnderCursor(MYPROJ2_TRACE_CHANNEL_WEAPON, false, CursorHit);

	// FireOrigin: actor origin (capsule centre, waist height). No vertical offset —
	// visual offsets are presentation concerns that belong on the weapon mesh,
	// not on the authoritative trace origin.
	const FVector FireOrigin = Owner->GetActorLocation();

	// Damageable targets use the actual 3D cursor impact, allowing targets above
	// or below the shooter to be hit. Ground/world aiming remains horizontal so
	// clicking nearby floor does not immediately drive the shot into the ground.
	FVector FireDirection;
	if (bHasCursorTarget)
	{
		FVector AimPoint = CursorHit.ImpactPoint;
		const AActor* CursorActor = CursorHit.GetActor();
		const bool bDamageableTarget = CursorActor
			&& CursorActor != Owner
			&& CursorActor->FindComponentByClass<UHealthComponent>();
		if (!bDamageableTarget)
		{
			AimPoint.Z = MuzzleLocation.Z;
		}

		FVector ToTarget = AimPoint - MuzzleLocation;
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

	// 3. Rate limit uses the server-derived build, never client-submitted stats.
	const float SecondsBetweenShots = DerivedStats.FireIntervalSeconds;
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

	// The Server derives the gameplay origin from its weapon. The client-supplied
	// muzzle is presentation/debug data only and cannot move the authoritative trace.
	const FVector TraceStart = Weapon ? Weapon->GetMuzzleLocation() : Owner->GetActorLocation();
	const float SpreadRadians = FMath::DegreesToRadians(DerivedStats.SpreadDegrees);
	const FVector TraceDirection = SpreadRadians > KINDA_SMALL_NUMBER
		? FMath::VRandCone(StartInfo.FireDirection, SpreadRadians)
		: StartInfo.FireDirection;
	const FVector TraceEnd = TraceStart + TraceDirection * DerivedStats.RangeCm;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MYPROJ2FireTrace), /*bTraceComplex=*/false, Owner);
	QueryParams.AddIgnoredActor(Owner);

	FHitResult Hit;
	// M3: hitscan uses the dedicated WeaponTrace channel (split from ECC_Visibility).
	// WeaponTrace is blocked by BlockAll/BlockAllDynamic/Pawn/PhysicsActor profiles
	// and ignored by Trigger/Spectator, matching the previous Visibility behavior
	// for combat-relevant geometry while letting interactables opt out.
	const ECollisionChannel FireChannel = MYPROJ2_TRACE_CHANNEL_WEAPON;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, FireChannel, QueryParams);

	// Apply damage on hit (server-authoritative).
	if (bHit && Hit.GetActor())
	{
		if (UHealthComponent* Health = Hit.GetActor()->FindComponentByClass<UHealthComponent>())
		{
			Health->ApplyDamage(DerivedStats.Damage);
			UE_LOG(LogMYPROJ2Combat, Log, TEXT("%s hit %s for %.1f damage"),
				*GetNameSafe(Owner), *GetNameSafe(Hit.GetActor()), DerivedStats.Damage);
		}
	}

	// Build firing context for multicast feedback.
	FFiringContext Ctx;
	Ctx.FireOrigin = TraceStart;
	Ctx.FireDirection = TraceDirection;
	Ctx.MuzzleSocket = TraceStart;
	Ctx.FireRate = 60.0f / FMath::Max(0.001f, DerivedStats.FireIntervalSeconds);
	Ctx.AmmoAfterShot = CurrentAmmo;

	MulticastFireFeedback(Ctx, Hit);
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), TraceStart, 1.0f, Owner,
		DerivedStats.NoiseRadiusCm, FName(TEXT("WeaponFire")));
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

UInventoryComponent* UCombatComponent::GetOwnerInventory() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	return Controller ? Controller->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void UCombatComponent::RebuildDerivedStats()
{
	if (!WeaponData)
	{
		return;
	}

	FWeaponStatBlock BaseStats = WeaponData->GetEffectiveBaseStats();
	TArray<FWeaponStatModifier> Modifiers;
	if (UInventoryComponent* Inventory = GetOwnerInventory())
	{
		for (const FInstalledWeaponPart& Installed : InstalledParts)
		{
			if (UWeaponPartDefinition* Part = Cast<UWeaponPartDefinition>(Inventory->ResolveDefinition(Installed.PartDefinitionId)))
			{
				Modifiers.Add(Part->Modifier);
			}
		}
	}
	DerivedStats = UWeaponStatLibrary::BuildStatsFromModifiers(BaseStats, Modifiers);
	CurrentAmmo = FMath::Min(CurrentAmmo, DerivedStats.MagazineSize);
	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

bool UCombatComponent::IsAssemblyRequestStale(uint16 RequestId) const
{
	if (!bHasConsumedAssemblyRequest)
	{
		return false;
	}
	const int32 Delta = static_cast<int32>(RequestId) - static_cast<int32>(LastConsumedAssemblyRequestId);
	const int32 WrappedDelta = ((Delta + 32768) % 65536 + 65536) % 65536 - 32768;
	return WrappedDelta <= 0;
}

void UCombatComponent::MarkAssemblyRequestConsumed(uint16 RequestId)
{
	LastConsumedAssemblyRequestId = RequestId;
	bHasConsumedAssemblyRequest = true;
}

void UCombatComponent::ServerInstallPart_Implementation(FGuid InventoryItemId, EWeaponPartSlot Slot, uint16 RequestId)
{
	EWeaponAssemblyRejectReason Reject = EWeaponAssemblyRejectReason::None;
	if (IsAssemblyRequestStale(RequestId))
	{
		Reject = EWeaponAssemblyRejectReason::StaleRequest;
	}
	else if (!bAllowRaidAssemblyForTesting)
	{
		Reject = EWeaponAssemblyRejectReason::NotAllowed;
	}
	else if (!WeaponData || !GetOwner() || !GetOwner()->HasAuthority())
	{
		Reject = EWeaponAssemblyRejectReason::NoWeapon;
	}
	else if (const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>(); Health && Health->IsDead())
	{
		Reject = EWeaponAssemblyRejectReason::Dead;
	}

	UInventoryComponent* Inventory = GetOwnerInventory();
	const FReplicatedInventoryEntry* Entry = Inventory ? Inventory->FindEntry(InventoryItemId) : nullptr;
	UWeaponPartDefinition* Part = Entry && Inventory ? Cast<UWeaponPartDefinition>(Inventory->ResolveDefinition(Entry->Item.DefinitionId)) : nullptr;
	if (Reject == EWeaponAssemblyRejectReason::None && !Inventory)
	{
		Reject = EWeaponAssemblyRejectReason::InvalidRequest;
	}
	else if (Reject == EWeaponAssemblyRejectReason::None && (!Entry || Entry->Item.Quantity != 1))
	{
		Reject = EWeaponAssemblyRejectReason::MissingItem;
	}
	else if (Reject == EWeaponAssemblyRejectReason::None && !Part)
	{
		Reject = EWeaponAssemblyRejectReason::InvalidDefinition;
	}
	else if (Reject == EWeaponAssemblyRejectReason::None && Part->Slot != Slot)
	{
		Reject = EWeaponAssemblyRejectReason::WrongSlot;
	}
	else if (Reject == EWeaponAssemblyRejectReason::None && !Part->CompatibleWeaponQuery.IsEmpty() &&
		!Part->CompatibleWeaponQuery.Matches(WeaponData->CompatibilityTags))
	{
		Reject = EWeaponAssemblyRejectReason::Incompatible;
	}
	else if (Reject == EWeaponAssemblyRejectReason::None && InstalledParts.ContainsByPredicate([Slot](const FInstalledWeaponPart& Item) { return Item.Slot == Slot; }))
	{
		Reject = EWeaponAssemblyRejectReason::SlotOccupied;
	}

	if (Reject != EWeaponAssemblyRejectReason::None)
	{
		UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("Weapon install rejected: reason=%d request=%hu"), static_cast<int32>(Reject), RequestId);
		ClientAssemblyRejected(RequestId, Reject);
		return;
	}

	FItemInstance RemovedItem;
	if (!Inventory->AuthorityTakeSingleInstance(InventoryItemId, RemovedItem))
	{
		UE_LOG(LogMYPROJ2Combat, Warning, TEXT("Weapon install failed while removing inventory item %s"), *InventoryItemId.ToString());
		return;
	}
	FInstalledWeaponPart Installed;
	Installed.Slot = Slot;
	Installed.SourceInstanceId = RemovedItem.InstanceId;
	Installed.PartDefinitionId = RemovedItem.DefinitionId;
	InstalledParts.Add(Installed);
	InstalledParts.Sort([](const FInstalledWeaponPart& A, const FInstalledWeaponPart& B)
	{
		return static_cast<uint8>(A.Slot) < static_cast<uint8>(B.Slot);
	});
	RebuildDerivedStats();
	MarkAssemblyRequestConsumed(RequestId);
}

void UCombatComponent::ServerRemovePart_Implementation(EWeaponPartSlot Slot, uint16 RequestId)
{
	if (IsAssemblyRequestStale(RequestId) || !bAllowRaidAssemblyForTesting || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const int32 PartIndex = InstalledParts.IndexOfByPredicate([Slot](const FInstalledWeaponPart& Item) { return Item.Slot == Slot; });
	if (PartIndex == INDEX_NONE)
	{
		return;
	}
	UInventoryComponent* Inventory = GetOwnerInventory();
	if (!Inventory)
	{
		return;
	}
	FItemInstance Restored;
	Restored.InstanceId = InstalledParts[PartIndex].SourceInstanceId;
	Restored.DefinitionId = InstalledParts[PartIndex].PartDefinitionId;
	Restored.Quantity = 1;
	EInventoryRejectReason InventoryReason = EInventoryRejectReason::None;
	if (!Inventory->AuthorityTryAddInstance(Restored, InventoryReason))
	{
		UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("Weapon remove rejected: inventory reason=%d"), static_cast<int32>(InventoryReason));
		ClientAssemblyRejected(RequestId, EWeaponAssemblyRejectReason::NoInventorySpace);
		return;
	}
	InstalledParts.RemoveAt(PartIndex);
	RebuildDerivedStats();
	MarkAssemblyRequestConsumed(RequestId);
}

void UCombatComponent::OnRep_InstalledParts()
{
	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("Installed weapon parts replicated: %d"), InstalledParts.Num());
}

void UCombatComponent::OnRep_DerivedStats()
{
	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("Derived weapon stats replicated: damage=%.1f interval=%.3f range=%.0f"),
		DerivedStats.Damage, DerivedStats.FireIntervalSeconds, DerivedStats.RangeCm);
}

void UCombatComponent::ClientAssemblyRejected_Implementation(uint16 RequestId, EWeaponAssemblyRejectReason Reason)
{
	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("Weapon assembly request %hu rejected: reason=%d"), RequestId, static_cast<int32>(Reason));
}
