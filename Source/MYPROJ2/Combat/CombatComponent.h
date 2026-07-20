#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "CombatComponent.generated.h"

class UWeaponData;
class AMYPROJ2Weapon;

/**
 * Authoritative fire validation + ammo pool. Lives on the owning Character.
 * Per m2-combat.md Step 4:
 *   - ServerTryFire: ammo / fire-rate / origin tolerance / direction validity.
 *   - Accepted fire decrements CurrentAmmo and multicasts feedback.
 *   - Rejected fire triggers ClientFireRejected (owning client only).
 *   - No reload in M2.
 */
UCLASS(ClassGroup=(MYPROJ2), meta=(BlueprintSpawnableComponent))
class MYPROJ2_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCombatComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	//~ Begin Config

	/** Data asset consumed for fire rate / damage / range / ammo pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UWeaponData> WeaponData;

	//~ End Config

	//~ Begin Replicated state

	/** Current ammo. Replicated to owner only per M2 doc. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo, BlueprintReadOnly, Category = "Combat")
	int32 CurrentAmmo = 0;

	//~ End Replicated state

	//~ Begin Client -> Server

	/**
	 * Owning client asks to fire. Reliable so ammo counter stays consistent.
	 * Validation is authoritative on Server; see m2-combat.md Step 4.
	 */
	UFUNCTION(Server, Reliable)
	void ServerTryFire(const FFireStartInfo& StartInfo);

	//~ End Client -> Server

	//~ Begin Server -> Client (owning only)

	/** Tells the shooter that the most recent trigger was rejected and why. */
	UFUNCTION(Client, Reliable)
	void ClientFireRejected(const FFireRejectionInfo& RejectionInfo);

	//~ End Server -> Client

	//~ Begin Server -> All

	/** Feedback for accepted fire: montage, tracer, impact, ammo counter. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFeedback(const FFiringContext& FiringContext, const FHitResult& ImpactResult);

	//~ End Server -> All

	/** Returns the Weapon actor currently attached to the owner, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	AMYPROJ2Weapon* GetWeapon() const { return Weapon; }

	/** Called by Character input; routes to ServerTryFire on owning client. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TryFire();

protected:

	UFUNCTION()
	void OnRep_CurrentAmmo(int32 PreviousAmmo);

	/** Server-only validation. Fills OutReason on failure. */
	bool ValidateFireRequest(const FFireStartInfo& StartInfo, EFireRejectReason& OutReason, FVector& OutExpectedOrigin) const;

	/** Server-only: performs the accepted hitscan and applies damage. */
	void ExecuteFire(const FFireStartInfo& StartInfo);

	/** Server-only: spawn weapon if not yet spawned. */
	void EnsureWeaponSpawned();

	/** Cached weapon actor; set by EnsureWeaponSpawned on Server, replicated via Owner. */
	UPROPERTY()
	TObjectPtr<AMYPROJ2Weapon> Weapon;

	/** Server-side timestamp of the most recent accepted fire, for rate limiting. */
	float LastAcceptedFireTime = 0.f;
};
