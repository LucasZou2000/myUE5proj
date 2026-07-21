#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "HealthComponent.generated.h"

/**
 * Replicated health pool. Server-owned; clients read via replication.
 * Per m2-combat.md Step 2:
 *   - Health replicated to all, CurrentAmmo replicated to owner only.
 *   - ApplyDamage is Server-only; call sites must check HasAuthority().
 *   - Zero clamp; no death logic in M2 (M4 scope).
 */
UCLASS(ClassGroup=(MYPROJ2), meta=(BlueprintSpawnableComponent))
class MYPROJ2_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ Begin Config

	/** Max hit points at spawn. Immutable per instance in M2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	//~ End Config

	//~ Begin State (replicated)

	/** Current hit points. Replicated to all; owned by Server. */
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Health")
	float Health = 0.f;

	//~ End State

	/** Server-only. Applies damage, clamps at zero, returns the new Health. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float ApplyDamage(float DamageAmount);

	/**
	 * Server-only. Applies healing, clamps at MaxHealth, returns the new Health.
	 * Authority-only; no client setter is exposed (m3-inventory-items.md).
	 */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float AuthorityHeal(float HealAmount);

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return Health <= 0.f; }

protected:

	UFUNCTION()
	void OnRep_Health(float PreviousHealth);

	virtual void BeginPlay() override;
};
